/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "windowsprintprovider.h"

#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include <QImage>
#include <QPageSize>

#include <objbase.h>
// needs to be included before any winrt header to enable classic COM support
#include <Unknwn.h>

#include <d2d1_1.h>
#include <d2d1helper.h>
#include <d3d11.h>
#include <wincodec.h>

#include <DocumentSource.h>
#include <PrintManagerInterop.h>
#include <PrintPreview.h>

#include <winrt/base.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.graphics.printing.h>

#include "global/runtime.h"
#include "global/async/async.h"

#include "draw/painter.h"

#include "notation/inotationpainting.h"

#include "log.h"

using namespace std::literals;
using namespace muse;
using namespace muse::async;
using namespace mu::notation;

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Printing;
}

using IPrintDocumentPackageTargetPtr = winrt::com_ptr<IPrintDocumentPackageTarget>;
using IPrintPreviewDxgiPackageTargetPtr = winrt::com_ptr<IPrintPreviewDxgiPackageTarget>;
using ID3D11DevicePtr = winrt::com_ptr<ID3D11Device>;
using ID2D1DevicePtr = winrt::com_ptr<ID2D1Device>;
using ID2D1DeviceContextPtr = winrt::com_ptr<ID2D1DeviceContext>;

namespace mu::print {
namespace {
// IPrintManagerInterop: https://learn.microsoft.com/en-us/windows/win32/api/printmanagerinterop/nn-printmanagerinterop-iprintmanagerinterop
class PrintManagerInterop
{
public:
    static winrt::PrintManager GetForWindow(const HWND window)
    {
        winrt::PrintManager printManager{ nullptr };
        get()->GetForWindow(window, winrt::guid_of<winrt::IPrintManager>(), winrt::put_abi(printManager));

        return printManager;
    }

    static winrt::IAsyncOperation<bool> ShowPrintUIForWindowAsync(const HWND window)
    {
        winrt::IAsyncOperation<bool> result{ nullptr };
        get()->ShowPrintUIForWindowAsync(window, winrt::guid_of<winrt::IAsyncOperation<bool> >(), winrt::put_abi(result));

        return result;
    }

    static winrt::com_ptr<IPrintManagerInterop> get()
    {
        return winrt::get_activation_factory<winrt::PrintManager, IPrintManagerInterop>();
    }
};

template<typename To, typename From>
To toWinRT(From* from)
{
    To to { nullptr };
    if (from) {
        from->QueryInterface(winrt::guid_of<To>(), winrt::put_abi(to));
    }

    return to;
}

std::future<std::vector<QImage> > paintNotationOnMainThread(const INotationPaintingPtr& notationPainting,
                                                            const QSize& pageSize)
{
    const auto promise = std::make_shared<std::promise<std::vector<QImage> > >();

    const auto task = [=] {
        std::vector<QImage> pageImages;
        QImage pageImage(pageSize, QImage::Format_RGBA8888);

        draw::Painter painter(&pageImage, "print");

        INotationPainting::Options paintOptions;
        paintOptions.fromPage = 0;
        paintOptions.toPage = notationPainting->pageCount() - 1;
        paintOptions.deviceDpi = pageImage.logicalDpiX();
        paintOptions.onNewPage = [&] {
            pageImages.push_back(pageImage);
        };
        notationPainting->paintPrint(&painter, paintOptions);

        painter.endDraw();

        // add final page
        pageImages.push_back(pageImage);

        promise->set_value(std::move(pageImages));
    };
    std::future<std::vector<QImage> > future = promise->get_future();

    Async::call(nullptr, task, runtime::mainThreadId());

    return future;
}

std::future<int> getPageCountOnMainThread(const INotationPaintingPtr& notationPainting)
{
    const auto promise = std::make_shared<std::promise<int> >();
    const auto task = [=] {
        promise->set_value(notationPainting->pageCount());
    };
    Async::call(nullptr, task, runtime::mainThreadId());

    return promise->get_future();
}

std::future<QImage> paintPageOnMainThread(const INotationPaintingPtr& painting,
                                          const QSize& pageSize, const int page)
{
    const auto promise = std::make_shared<std::promise<QImage> >();
    auto task = [=] {
        QImage pageImage(pageSize, QImage::Format_RGBA8888);

        draw::Painter painter(&pageImage, "print");

        INotationPainting::Options paintOptions;
        paintOptions.fromPage = page;
        paintOptions.toPage = page;
        paintOptions.deviceDpi = pageImage.logicalDpiX();
        painting->paintPrint(&painter, paintOptions);

        painter.endDraw();

        promise->set_value(std::move(pageImage));
    };

    Async::call(nullptr, std::move(task), runtime::mainThreadId());

    return promise->get_future();
}

class NotationPrintRenderer;
using NotationPrintRendererPtr = std::shared_ptr<NotationPrintRenderer>;

// accessed from multiple threads
class NotationPrintRenderer
{
public:
    static NotationPrintRendererPtr make()
    {
        D3D_FEATURE_LEVEL d3dFeatureLevel = {};
        winrt::com_ptr<ID3D11Device> d3dDevice;
        winrt::com_ptr<ID3D11DeviceContext> d3dImmediateCtx;
        HRESULT hr = D3D11CreateDevice(nullptr,
                                       D3D_DRIVER_TYPE_WARP,
                                       nullptr,
                                       D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                       nullptr,
                                       0,
                                       D3D11_SDK_VERSION,
                                       d3dDevice.put(),
                                       &d3dFeatureLevel,
                                       d3dImmediateCtx.put());
        if (FAILED(hr)) {
            return nullptr;
        }

        winrt::com_ptr<ID2D1Factory1> d2dFactory;
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, d2dFactory.put());
        if (FAILED(hr)) {
            return nullptr;
        }

        const auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
        winrt::com_ptr<ID2D1Device> d2dDevice;
        hr = d2dFactory->CreateDevice(dxgiDevice.get(), d2dDevice.put());
        if (FAILED(hr)) {
            return nullptr;
        }

        winrt::com_ptr<ID2D1DeviceContext> d2dDeviceCtx;
        hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dDeviceCtx.put());
        if (FAILED(hr)) {
            return nullptr;
        }

        return std::make_shared<NotationPrintRenderer>(std::move(d3dDevice), std::move(d2dDevice), std::move(d2dDeviceCtx));
    }

    NotationPrintRenderer(ID3D11DevicePtr d3dDevice, ID2D1DevicePtr device, ID2D1DeviceContextPtr deviceCtx)
        : m_d3dDevice(std::move(d3dDevice)), m_device(std::move(device)), m_deviceCtx(std::move(deviceCtx))
    {
        DO_ASSERT(m_d3dDevice);
        DO_ASSERT(m_device);
        DO_ASSERT(m_deviceCtx);
    }

    HRESULT printNotation(const INotationPaintingPtr& notationPainting, const winrt::PrintTaskOptions& options,
                          const IPrintDocumentPackageTargetPtr& target)
    {
        const winrt::PrintPageDescription pageDesc = options.GetPageDescription(1);
        const winrt::Size pageSize = pageDesc.PageSize;
        const QSize pageImageSize = QSizeF(pageSize.Width, pageSize.Height)
                                    .toSize();
        auto pageImagesFuture = paintNotationOnMainThread(notationPainting, pageImageSize);

        winrt::com_ptr<IWICImagingFactory> wicImagingFactory;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wicImagingFactory.put()));
        if (FAILED(hr)) {
            return hr;
        }

        winrt::com_ptr<ID2D1PrintControl> d2dPrintControl;
        hr = m_device->CreatePrintControl(wicImagingFactory.get(), target.get(), nullptr, d2dPrintControl.put());
        if (FAILED(hr)) {
            return hr;
        }

        const D2D1_PIXEL_FORMAT d2dPagePixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT);

        for (const QImage& pageImage : pageImagesFuture.get()) {
            winrt::com_ptr<ID2D1CommandList> d2dCmdList;
            hr = m_deviceCtx->CreateCommandList(d2dCmdList.put());
            if (FAILED(hr)) {
                return hr;
            }

            m_deviceCtx->SetTarget(d2dCmdList.get());
            m_deviceCtx->SetTransform(D2D1::Matrix3x2F::Identity());

            winrt::com_ptr<ID2D1Bitmap> d2dPageBitmap;
            m_deviceCtx->CreateBitmap(D2D1::SizeU(pageImage.width(), pageImage.height()),
                                      pageImage.constBits(), pageImage.bytesPerLine(),
                                      D2D1::BitmapProperties(d2dPagePixelFormat),
                                      d2dPageBitmap.put());

            m_deviceCtx->BeginDraw();

            const D2D1_RECT_F d2dPageRect = D2D1::RectF(0.0f, 0.0f, pageSize.Width, pageSize.Height);

            m_deviceCtx->DrawBitmap(d2dPageBitmap.get(), d2dPageRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);

            HRESULT drawResult = m_deviceCtx->EndDraw();
            if (FAILED(drawResult)) {
                LOGE() << "drawing failed";
            }

            hr = d2dCmdList->Close();
            if (FAILED(hr)) {
                LOGE() << "closing cmd list failed";

                return hr;
            }
            m_deviceCtx->SetTarget(nullptr);

            hr = d2dPrintControl->AddPage(d2dCmdList.get(), D2D1::SizeF(pageSize.Width, pageSize.Height), nullptr);
            if (FAILED(hr)) {
                LOGE() << "failed to add page";

                return hr;
            }
        }

        hr = d2dPrintControl->Close();
        if (FAILED(hr)) {
            return hr;
        }

        return S_OK;
    }

    HRESULT renderPreviewPage(const INotationPaintingPtr& notationPainting, const winrt::PrintPageDescription& pageDesc,
                              const IPrintPreviewDxgiPackageTargetPtr& target,
                              const UINT32 page)
    {
        const winrt::Size pageSize = pageDesc.PageSize;
        const QSize pageImageSize = QSizeF(pageSize.Width, pageSize.Height)
                                    .toSize();
        const QImage pageImage = paintPageOnMainThread(notationPainting, pageImageSize, static_cast<int>(page) - 1)
                                 .get();
        DO_ASSERT(pageImage.format() == QImage::Format_RGBA8888);

        const CD3D11_TEXTURE2D_DESC texDesc(DXGI_FORMAT_R8G8B8A8_UNORM, pageImage.width(), pageImage.height(), 1, 1,
                                            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = pageImage.constBits();
        data.SysMemPitch = pageImage.bytesPerLine();
        winrt::com_ptr<ID3D11Texture2D> pageTexture;
        HRESULT hr = m_d3dDevice->CreateTexture2D(&texDesc, &data, pageTexture.put());
        if (FAILED(hr)) {
            LOGE() << "failed to create texture";
            return hr;
        }
        const auto pageSurface = pageTexture.as<IDXGISurface>();

        return target->DrawPage(page, pageSurface.get(), pageImage.logicalDpiX(), pageImage.logicalDpiY());
    }

private:
    ID3D11DevicePtr m_d3dDevice;
    ID2D1DevicePtr m_device;
    ID2D1DeviceContextPtr m_deviceCtx;
};

// IPrintPreviewPageCollection: https://learn.microsoft.com/en-us/previous-versions/jj710018(v=vs.85)
class NotationPrintPreviewPageCollection : public winrt::implements<NotationPrintPreviewPageCollection,
                                                                    IPrintPreviewPageCollection>
{
public:
    explicit NotationPrintPreviewPageCollection(NotationPrintRendererPtr renderer, INotationPaintingPtr notationPainting,
                                                IPrintPreviewDxgiPackageTargetPtr target)
        : m_renderer(std::move(renderer)), m_notationPainting(std::move(notationPainting)), m_target(std::move(target))
    {
        DO_ASSERT(m_renderer);
        DO_ASSERT(m_notationPainting);
        DO_ASSERT(m_target);
    }

    HRESULT __stdcall Paginate(UINT32 /*currentJobPage*/, ::IInspectable* printTaskOptions) final
    {
        std::lock_guard lock(m_mutex);

        if (!printTaskOptions) {
            return E_INVALIDARG;
        }

        m_options = toWinRT<winrt::PrintTaskOptions>(printTaskOptions);

        // TODO: fetch this in WindowsPrintProvider::printNotation
        const int pageCount = getPageCountOnMainThread(m_notationPainting).get();

        return m_target->SetJobPageCount(FinalPageCount, static_cast<UINT32>(pageCount));
    }

    HRESULT __stdcall MakePage(UINT32 desiredJobPage, FLOAT /*width*/, FLOAT /*height*/) final
    {
        std::lock_guard lock(m_mutex);

        if (!m_options) {
            LOGE() << "missing task options";
            return E_FAIL;
        }

        const UINT32 page = desiredJobPage == JOB_PAGE_APPLICATION_DEFINED ? 1 : desiredJobPage;

        return m_renderer->renderPreviewPage(m_notationPainting, m_options.GetPageDescription(page), m_target, page);
    }

private:
    std::mutex m_mutex;
    NotationPrintRendererPtr m_renderer;
    INotationPaintingPtr m_notationPainting;
    IPrintPreviewDxgiPackageTargetPtr m_target;
    winrt::PrintTaskOptions m_options{ nullptr };
};

// this class must be thread-safe because its methods are called from worker threads
// IPrintDocumentSource: https://learn.microsoft.com/en-us/uwp/api/windows.graphics.printing.iprintdocumentsource
// IPrintDocumentPageSource: https://learn.microsoft.com/en-us/previous-versions/jj710015(v=vs.85)
class NotationPrintDocumentSource : public winrt::implements<NotationPrintDocumentSource,
                                                             winrt::IPrintDocumentSource,
                                                             IPrintDocumentPageSource>
{
public:
    explicit NotationPrintDocumentSource(const INotationPtr& notation)
        : m_notation(notation) {}

    HRESULT __stdcall GetPreviewPageCollection(IPrintDocumentPackageTarget* docPackageTarget,
                                               IPrintPreviewPageCollection** docPageCollection) final
    {
        if (!docPackageTarget) {
            LOGE() << "missing document package target";
            return E_INVALIDARG;
        }

        winrt::com_ptr<IPrintPreviewDxgiPackageTarget> previewTarget;
        docPackageTarget->GetPackageTarget(ID_PREVIEWPACKAGETARGET_DXGI, IID_PPV_ARGS(previewTarget.put()));
        if (!previewTarget) {
            return E_NOINTERFACE;
        }

        if (!docPageCollection) {
            LOGE() << "missing preview page collection";
            return E_INVALIDARG;
        }

        const NotationPrintRendererPtr& renderer = getRenderer();
        if (!renderer) {
            LOGE() << "failed to get renderer";
            return E_FAIL;
        }

        auto pageCollection = winrt::make<NotationPrintPreviewPageCollection>(renderer, m_notation->painting(), previewTarget);
        *docPageCollection = pageCollection.detach();

        return S_OK;
    }

    HRESULT __stdcall MakeDocument(::IInspectable* printTaskOptions, IPrintDocumentPackageTarget* docPackageTarget) final
    {
        if (!printTaskOptions) {
            LOGE() << "missing task options";
            return E_INVALIDARG;
        }

        const auto options = toWinRT<winrt::PrintTaskOptions>(printTaskOptions);

        if (!docPackageTarget) {
            LOGE() << "missing document package target";
            return E_INVALIDARG;
        }

        IPrintDocumentPackageTargetPtr target;
        target.copy_from(docPackageTarget);

        const NotationPrintRendererPtr& renderer = getRenderer();
        if (!renderer) {
            LOGE() << "failed to get renderer";
            return E_FAIL;
        }

        return renderer->printNotation(m_notation->painting(), options, target);
    }

private:
    const NotationPrintRendererPtr& getRenderer()
    {
        if (!m_renderer) {
            m_renderer = NotationPrintRenderer::make();
        }

        return m_renderer;
    }

    // only access notation on main thread
    INotationPtr m_notation;
    NotationPrintRendererPtr m_renderer;
};

winrt::PrintMediaSize toPrintMediaSize(const SizeF& pageSizeInch)
{
    const auto pageSizeId = QPageSize::id(pageSizeInch.toQSizeF(), QPageSize::Inch, QPageSize::FuzzyOrientationMatch);
    switch (pageSizeId) {
    case QPageSize::Letter:
        return winrt::PrintMediaSize::NorthAmericaLetter;
    case QPageSize::Legal:
        return winrt::PrintMediaSize::NorthAmericaLegal;
    case QPageSize::Executive:
        return winrt::PrintMediaSize::NorthAmericaExecutive;
    case QPageSize::A0:
        return winrt::PrintMediaSize::IsoA0;
    case QPageSize::A1:
        return winrt::PrintMediaSize::IsoA1;
    case QPageSize::A2:
        return winrt::PrintMediaSize::IsoA2;
    case QPageSize::A3:
        return winrt::PrintMediaSize::IsoA3;
    case QPageSize::A4:
        return winrt::PrintMediaSize::IsoA4;
    case QPageSize::A5:
        return winrt::PrintMediaSize::IsoA5;
    case QPageSize::A6:
        return winrt::PrintMediaSize::IsoA6;
    case QPageSize::A7:
        return winrt::PrintMediaSize::IsoA7;
    case QPageSize::A8:
        return winrt::PrintMediaSize::IsoA8;
    case QPageSize::A9:
        return winrt::PrintMediaSize::IsoA9;
    case QPageSize::A10:
        return winrt::PrintMediaSize::IsoA10;
    default:
        return winrt::PrintMediaSize::Default;
    }
}
}

bool WindowsPrintProvider::isAvailable()
{
    return winrt::PrintManager::IsSupported();
}

WindowsPrintProvider::WindowsPrintProvider(const modularity::ContextPtr& ctx)
    : Injectable(ctx)
{
}

Promise<Ret> WindowsPrintProvider::printNotation(const INotationPtr notation)
{
    return Promise<Ret>([&] (const auto resolve) {
        IF_ASSERT_FAILED(notation) {
            return resolve(muse::make_ret(Ret::Code::InternalError, "can't print null notation"s));
        }

        const SizeF pageSizeInch = notation->painting()->pageSizeInch();
        const winrt::PrintMediaSize mediaSize = toPrintMediaSize(pageSizeInch);
        const auto orientation = pageSizeInch.height() > pageSizeInch.width()
                                 ? winrt::PrintOrientation::Portrait
                                 : winrt::PrintOrientation::Landscape;

        const auto mainWindowHandle = reinterpret_cast<HWND>(mainWindow()->qWindow()->winId());
        const winrt::PrintManager printManager = PrintManagerInterop::GetForWindow(mainWindowHandle);

        const auto taskRequestedToken = printManager.PrintTaskRequested(
            [=, title = notation->projectWorkTitleAndPartName()](const winrt::PrintManager&,
                                                                 const winrt::PrintTaskRequestedEventArgs& event)
        {
            const winrt::PrintTask task = event.Request().CreatePrintTask(
                title.toStdWString(), [=](const winrt::PrintTaskSourceRequestedArgs& event) {
                event.SetSource(winrt::make<NotationPrintDocumentSource>(notation));
            });
            task.Completed([=](const winrt::PrintTask&, const winrt::PrintTaskCompletedEventArgs& event) {
                LOGD() << "print task completed";

                const winrt::PrintTaskCompletion completion = event.Completion();
                switch (completion) {
                    case winrt::PrintTaskCompletion::Abandoned:
                        (void)resolve(muse::make_ret(Ret::Code::InternalError, "Abandoned"s));
                        return;
                    case winrt::PrintTaskCompletion::Failed:
                        (void)resolve(muse::make_ret(Ret::Code::InternalError, "Failed"s));
                        return;
                    case winrt::PrintTaskCompletion::Canceled:
                        (void)resolve(muse::make_ret(Ret::Code::Cancel));
                        return;
                    case winrt::PrintTaskCompletion::Submitted:
                        (void)resolve(muse::make_ok());
                        return;
                }

                std::ostringstream msg;
                msg << "unknown PrintTaskCompletion: " << static_cast<int>(completion);
                (void)resolve(muse::make_ret(Ret::Code::InternalError, msg.str()));
            });

            const winrt::PrintTaskOptions options = task.Options();
            const winrt::IVector<winrt::hstring> displayedOptions = task.Options().DisplayedOptions();
            displayedOptions.Clear();
            displayedOptions.Append(winrt::StandardPrintTaskOptions::Orientation());
            displayedOptions.Append(winrt::StandardPrintTaskOptions::Copies());
            displayedOptions.Append(winrt::StandardPrintTaskOptions::ColorMode());

            displayedOptions.Append(winrt::StandardPrintTaskOptions::CustomPageRanges());
            const winrt::PrintPageRangeOptions pageRangeOptions = options.PageRangeOptions();
            pageRangeOptions.AllowAllPages(true);
            pageRangeOptions.AllowCustomSetOfPages(true);
            pageRangeOptions.AllowCurrentPage(false);

            options.MediaSize(mediaSize);
            options.Orientation(orientation);
        });

        PrintManagerInterop::ShowPrintUIForWindowAsync(mainWindowHandle)
        .Completed([=] (const winrt::IAsyncOperation<bool>&, winrt::AsyncStatus) {
            printManager.PrintTaskRequested(taskRequestedToken);
        });
        return Promise<Ret>::dummy_result();
    }, PromiseType::AsyncByBody);
}
}
