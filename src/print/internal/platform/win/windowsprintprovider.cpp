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
#include <sstream>
#include <vector>

#include <QImage>

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
using namespace Windows::Graphics::Printing;
}

using IPrintDocumentPackageTargetPtr = winrt::com_ptr<IPrintDocumentPackageTarget>;

namespace mu::print {
namespace {
// wrapper for the IPrintManagerInterop interface
// IPrintManagerInterop provides access to the PrintManager and the modern print UI without needing a CoreApplicationView
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

std::future<std::vector<QImage> > paintNotationOnMainThread(const INotationPtr& notation,
                                                            const winrt::PrintTaskOptions& options)
{
    std::packaged_task<std::vector<QImage>()> task([=] {
        const INotationPaintingPtr painting = notation->painting();

        const winrt::PrintPageDescription pageDesc = options.GetPageDescription(1);
        const winrt::Size pageSize = pageDesc.PageSize;

        std::vector<QImage> pageImages;
        const QSize pageImageSize(pageSize.Width, pageSize.Height);
        QImage pageImage(pageImageSize, QImage::Format_RGBX8888);

        draw::Painter painter(&pageImage, "print");

        INotationPainting::Options paintOptions;
        paintOptions.fromPage = 0;
        paintOptions.toPage = painting->pageCount() - 1;
        paintOptions.copyCount = 1; //printer->supportsMultipleCopies() ? 1 : printer->copyCount();
        paintOptions.deviceDpi = pageImage.logicalDpiX();
        paintOptions.onNewPage = [&]() {
            pageImages.push_back(pageImage);
        };

        painting->paintPrint(&painter, paintOptions);
        painter.endDraw();

        // add final page
        pageImages.push_back(std::move(pageImage));

        return pageImages;
    });
    std::future<std::vector<QImage> > future = task.get_future();

    Async::call(nullptr, std::move(task), runtime::mainThreadId());

    return future;
}

HRESULT printNotation(const INotationPtr& notation, const IPrintDocumentPackageTargetPtr& target,
                      const winrt::PrintTaskOptions& options)
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
        return hr;
    }

    winrt::com_ptr<ID2D1Factory1> d2dFactory;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, d2dFactory.put());
    if (FAILED(hr)) {
        return hr;
    }

    const auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    winrt::com_ptr<ID2D1Device> d2dDevice;
    hr = d2dFactory->CreateDevice(dxgiDevice.get(), d2dDevice.put());
    if (FAILED(hr)) {
        return hr;
    }

    winrt::com_ptr<IWICImagingFactory> wicImagingFactory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wicImagingFactory.put()));
    if (FAILED(hr)) {
        return hr;
    }

    winrt::com_ptr<ID2D1PrintControl> d2dPrintControl;
    hr = d2dDevice->CreatePrintControl(wicImagingFactory.get(), target.get(), nullptr, d2dPrintControl.put());
    if (FAILED(hr)) {
        return hr;
    }

    winrt::com_ptr<ID2D1DeviceContext> d2dDeviceCtx;
    hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dDeviceCtx.put());
    if (FAILED(hr)) {
        return hr;
    }

    const winrt::PrintPageDescription pageDesc = options.GetPageDescription(1);
    const winrt::Size pageSize = pageDesc.PageSize;
    const uint32_t pageDpi = pageDesc.DpiX;
    const D2D1_PIXEL_FORMAT d2dPagePixelFormat = D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);

    const std::vector<QImage> pageImages = paintNotationOnMainThread(notation, options).get();
    for (const QImage& pageImage : pageImages) {
        winrt::com_ptr<ID2D1CommandList> d2dCmdList;
        hr = d2dDeviceCtx->CreateCommandList(d2dCmdList.put());
        if (FAILED(hr)) {
            return hr;
        }

        d2dDeviceCtx->SetTarget(d2dCmdList.get());
        d2dDeviceCtx->SetTransform(D2D1::Matrix3x2F::Identity());

        winrt::com_ptr<ID2D1Bitmap> d2dPageBitmap;
        d2dDeviceCtx->CreateBitmap(D2D1::SizeU(pageImage.width(), pageImage.height()),
                                   pageImage.constBits(), pageImage.bytesPerLine(),
                                   D2D1::BitmapProperties(d2dPagePixelFormat),
                                   d2dPageBitmap.put());

        d2dDeviceCtx->BeginDraw();

        const D2D1_RECT_F d2dPageRect = D2D1::RectF(0.0f, 0.0f, pageSize.Width, pageSize.Height);

        d2dDeviceCtx->DrawBitmap(d2dPageBitmap.get(), d2dPageRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);

        HRESULT drawResult = d2dDeviceCtx->EndDraw();
        if (FAILED(drawResult)) {
            LOGE() << "drawing failed";
        }

        hr = d2dCmdList->Close();
        if (FAILED(hr)) {
            LOGE() << "closing cmd list failed";

            return hr;
        }
        d2dDeviceCtx->SetTarget(nullptr);

        hr = d2dPrintControl->AddPage(d2dCmdList.get(), D2D1::SizeF(pageSize.Width, pageSize.Height), nullptr);
        if (FAILED(hr)) {
            LOGE() << "adding page failed";

            return hr;
        } else {
            LOGI() << "added page";
        }
    }

    hr = d2dPrintControl->Close();
    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}

// this class must be thread-safe because its methods are called from worker threads
class NotationPrintDocumentSource : public winrt::implements<NotationPrintDocumentSource,
                                                             winrt::IPrintDocumentSource,
                                                             IPrintDocumentPageSource>
{
public:
    explicit NotationPrintDocumentSource(const INotationPtr& notation)
        : m_notation(notation) {}

    // IPrintDocumentSource: https://learn.microsoft.com/en-us/uwp/api/windows.graphics.printing.iprintdocumentsource?view=winrt-26100

    // IPrintDocumentPageSource: https://learn.microsoft.com/en-us/previous-versions/jj710015(v=vs.85)
    HRESULT __stdcall GetPreviewPageCollection([[maybe_unused]] IPrintDocumentPackageTarget* docPackageTarget,
                                               [[maybe_unused]] IPrintPreviewPageCollection** docPageCollection) final
    {
        // if (!docPackageTarget) {
        //     return E_INVALIDARG;
        // }

        // winrt::com_ptr<IPrintPreviewDxgiPackageTarget> previewTarget;
        // docPackageTarget->GetPackageTarget(ID_PREVIEWPACKAGETARGET_DXGI, IID_PPV_ARGS(previewTarget.put()));
        // if (!previewTarget) {
        //     return E_NOINTERFACE;
        // }

        return E_NOTIMPL;
    }

    HRESULT __stdcall MakeDocument(::IInspectable* printTaskOptions, IPrintDocumentPackageTarget* docPackageTarget) final
    {
        if (!printTaskOptions) {
            LOGE() << "no print options";
            return E_INVALIDARG;
        }
        if (!docPackageTarget) {
            LOGE() << "no print target";
            return E_INVALIDARG;
        }
        const INotationPtr notation = m_notation.lock();
        if (!notation) {
            LOGE() << "notation expired";
            return E_FAIL;
        }

        IPrintDocumentPackageTargetPtr target;
        target.copy_from(docPackageTarget);

        const auto options = toWinRT<winrt::PrintTaskOptions>(printTaskOptions);

        return printNotation(notation, target, options);
    }

private:
    // only access on main thread
    INotationWeakPtr m_notation;
};

using ResolvePrint = Promise<Ret>::Resolve;

class NotationPrintHandler;
using NotationPrintHandlerPtr = std::shared_ptr<NotationPrintHandler>;

// this class must be thread-safe because the event handlers are called from worker threads
class NotationPrintHandler : public std::enable_shared_from_this<NotationPrintHandler>
{
public:
    // must be called from the main thread
    static NotationPrintHandlerPtr makeAndRegister(const HWND window, INotationPtr notation)
    {
        NotationPrintHandlerPtr handler = std::make_shared<NotationPrintHandler>(window, std::move(notation));

        const winrt::PrintManager printManager = PrintManagerInterop::GetForWindow(window);
        handler->m_eventHandlerRevoker = printManager.PrintTaskRequested(
            winrt::auto_revoke, { std::weak_ptr(handler), &NotationPrintHandler::onPrintTaskRequested });

        return handler;
    }

    // must be called from the main thread
    NotationPrintHandler(const HWND window, INotationPtr notation)
        : m_window(window),
        m_title(notation->projectWorkTitleAndPartName().toStdWString()),
        m_notation(std::move(notation))
    {
        DO_ASSERT(m_window);
    }

    Promise<Ret> requestPrintAndShowUi()
    {
        return Promise<Ret>([&] (const auto resolve) {
            m_resolve = resolve;
            try {
                winrt::IAsyncOperation<bool> op = PrintManagerInterop::ShowPrintUIForWindowAsync(m_window);
                op.Completed([=] (const winrt::IAsyncOperation<bool>&, const winrt::AsyncStatus status) {
                    if (status != winrt::AsyncStatus::Completed) {
                        (void)resolve(muse::make_ret(Ret::Code::InternalError, "failed to show print UI"s));
                    }
                });
            } catch (const winrt::hresult_error& ex) {
                return resolve(muse::make_ret(Ret::Code::InternalError,
                                              "failed to show print UI: "s + winrt::to_string(ex.message())));
            }

            return Promise<Ret>::dummy_result();
        }, PromiseType::AsyncByBody);
    }

private:
    void onPrintTaskRequested(const winrt::PrintManager&, const winrt::PrintTaskRequestedEventArgs& event) const
    {
        const winrt::PrintTaskRequest request = event.Request();
        const winrt::PrintTask task = request.CreatePrintTask(
            m_title, { weak_from_this(), &NotationPrintHandler::onPrintTaskSourceRequested });

        // TODO: set displayedOptions
        // TODO: set options defaults
        task.IsPreviewEnabled(false);

        task.Completed({ weak_from_this(), &NotationPrintHandler::onPrintTaskCompleted });
    }

    void onPrintTaskSourceRequested(const winrt::PrintTaskSourceRequestedArgs& event) const
    {
        if (const INotationPtr notation = m_notation.lock()) {
            event.SetSource(winrt::make<NotationPrintDocumentSource>(notation));
        } else {
            LOGE() << "notation expired";
        }
    }

    void onPrintTaskCompleted(const winrt::PrintTask&, const winrt::PrintTaskCompletedEventArgs& event) const
    {
        const winrt::PrintTaskCompletion completion = event.Completion();
        switch (completion) {
        case winrt::PrintTaskCompletion::Abandoned:
            (void)m_resolve(muse::make_ret(Ret::Code::InternalError, "Abandoned"s));
            return;
        case winrt::PrintTaskCompletion::Failed:
            (void)m_resolve(muse::make_ret(Ret::Code::InternalError, "Failed"s));
            return;
        case winrt::PrintTaskCompletion::Canceled:
            (void)m_resolve(muse::make_ret(Ret::Code::Cancel));
            return;
        case winrt::PrintTaskCompletion::Submitted:
            (void)m_resolve(muse::make_ok());
            return;
        }

        std::ostringstream msg;
        msg << "unknown PrintTaskCompletion: " << static_cast<int>(completion);
        (void)m_resolve(muse::make_ret(Ret::Code::InternalError, msg.str()));
    }

    winrt::PrintManager::PrintTaskRequested_revoker m_eventHandlerRevoker;

    HWND m_window;
    winrt::hstring m_title;

    // only access notation on main thread
    INotationWeakPtr m_notation;

    ResolvePrint m_resolve;
};
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

        const auto window = this->mainWindow();
        IF_ASSERT_FAILED(window) {
            return resolve(muse::make_ret(Ret::Code::InternalError, "can't print without main window"s));
        }

        const auto windowHandle = reinterpret_cast<HWND>(window->qWindow()->winId());
        NotationPrintHandlerPtr printHandler = NotationPrintHandler::makeAndRegister(windowHandle, notation);

        Promise<Ret> printHandlerPromise = printHandler->requestPrintAndShowUi();
        printHandlerPromise.onResolve(
            nullptr, [=, printHandler = std::move(printHandler)] (const Ret& ret) mutable {
            (void)resolve(ret);

            // unregister and destroy printHandler
            // needed to break circular dependency
            // - the stored resolve callback in NotationPrintHandler keeps the callbacks alive
            // - this callback keeps the NotationPrintHandler alive
            printHandler.reset();
        });

        return Promise<Ret>::dummy_result();
    }, PromiseType::AsyncByBody);
}
}
