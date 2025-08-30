/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
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

#include <gtest/gtest.h>

#include <memory>
#include <string_view>

#include "global/stringutils.h"

#include "engraving/dom/barline.h"
#include "engraving/dom/bracket.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/system.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/undo.h"

#include "utils/scorerw.h"
#include "utils/scorecomp.h"

using namespace std::literals;
using namespace muse;

namespace mu::engraving {
using MasterScorePtr = std::unique_ptr<const MasterScore>;
using MutMasterScorePtr = std::unique_ptr<MasterScore>;

namespace {
constexpr std::string_view BARLINE_DATA_DIR = "barline_data"sv;
constexpr track_idx_t FIRST_TRACK_IDX = 0;

MutMasterScorePtr readMutScore(const std::string_view fileName)
{
    const std::string filePath = strings::concat(BARLINE_DATA_DIR, "/"sv, fileName);
    MasterScore* score = ScoreRW::readScore(String::fromUtf8(filePath));

    return MutMasterScorePtr { score };
}

MasterScorePtr readScore(const std::string_view fileName)
{
    return readMutScore(fileName);
}
}

class Engraving_BarlineTests : public ::testing::Test
{
};

// Check bar line and brackets presence and length with hidden empty staves:
//   A score with:
//         3 staves,
//         bracket across all 3 staves
//         bar lines across all 3 staves
//         systems with each staff hidden in turn because empty
//   is loaded, laid out and bracket/bar line sizes are checked.
// TODO: verifying the height should be done by vtests / layout tests
TEST_F(Engraving_BarlineTests, barline01)
{
    // actual 3-staff bracket should be high 28.6 SP ca.: allow for some layout margin
    static constexpr double BRACKET0_HEIGHT_MIN = 27.0;
    static constexpr double BRACKET0_HEIGHT_MAX = 30.0;
    // actual 2-staff bracket should be high 18.1 SP ca.
    static constexpr double BRACKET_HEIGHT_MIN = 17.0;
    static constexpr double BRACKET_HEIGHT_MAX = 20.0;

    const MasterScorePtr score = readScore("barline01.mscx"sv);
    ASSERT_TRUE(score);

    const double spatium = score->style().spatium();
    bool isFirstSystem = true;
    for (const System* sys : score->systems()) {
        // check number of the brackets of each system
        ASSERT_EQ(sys->brackets().size(), 1);

        // check height of the bracket of each system
        // (bracket height is different between first system (3 staves) and other systems (2 staves) )
        const Bracket* bracket = sys->brackets().at(0);
        const double height = bracket->ldata()->bbox().height() / spatium;
        const double heightMin = isFirstSystem ? BRACKET0_HEIGHT_MIN : BRACKET_HEIGHT_MIN;
        const double heightMax = isFirstSystem ? BRACKET0_HEIGHT_MAX : BRACKET_HEIGHT_MAX;

        EXPECT_GT(height, heightMin);
        EXPECT_LT(height, heightMax);

        // check presence and height of the bar line of each measure of each system
        // TODO: where is the span/height checked?
        for (MeasureBase* mb : sys->measures()) {
            const Measure* msr = toMeasure(mb);
            const Segment* seg = msr->findSegment(SegmentType::EndBarLine, mb->endTick());
            ASSERT_TRUE(seg);

            const BarLine* bar = toBarLine(seg->element(FIRST_TRACK_IDX));
            EXPECT_TRUE(bar);
        }

        isFirstSystem = false;
    }
}

// add a 3/4 time signature in the second measure and check bar line 'generated' status
TEST_F(Engraving_BarlineTests, barline02)
{
    const MutMasterScorePtr score = readMutScore("barline02.mscx"sv);
    ASSERT_TRUE(score);

    Measure* msr = score->firstMeasure()->nextMeasure();
    TimeSig* ts  = Factory::createTimeSig(score->dummy()->segment());
    ts->setSig(Fraction(3, 4), TimeSigType::NORMAL);

    constexpr bool isLocal = false;
    score->cmdAddTimeSig(msr, staff_idx_t { 0 }, ts, isLocal);
    score->doLayout();

    for (const Measure* m = score->firstMeasure()->nextMeasure(); m; m = m->nextMeasure()) {
        const Segment* seg = m->findSegment(SegmentType::EndBarLine, m->endTick());
        ASSERT_TRUE(seg);

        const BarLine* bar = toBarLine(seg->element(FIRST_TRACK_IDX));
        ASSERT_TRUE(bar);

        // bar line should be generated if NORMAL, except the END one at the end
        EXPECT_TRUE(bar->generated());
    }
}

// Sets a staff bar line span involving spanFrom and spanTo and
// check that it is properly applied to start-repeat
TEST_F(Engraving_BarlineTests, barline03)
{
    const MutMasterScorePtr score = readMutScore("barline03.mscx"sv);
    ASSERT_TRUE(score);

    Staff* firstStaff = score->staff(0);
    score->startCmd(TranslatableString::untranslatable("Engraving barline tests"));
    firstStaff->undoChangeProperty(Pid::STAFF_BARLINE_SPAN, 1);
    firstStaff->undoChangeProperty(Pid::STAFF_BARLINE_SPAN_FROM, 2);
    firstStaff->undoChangeProperty(Pid::STAFF_BARLINE_SPAN_TO, -2);
    score->endCmd();

    // 'go' to 5th measure
    const Measure* msr = score->firstMeasure();
    for (int i = 0; i < 4; i++) {
        msr = msr->nextMeasure();
    }
    // check span data of measure-initial start-repeat bar line
    const Segment* seg = msr->findSegment(SegmentType::StartRepeatBarLine, msr->tick());
    ASSERT_TRUE(seg);

    const BarLine* bar = toBarLine(seg->element(FIRST_TRACK_IDX));
    EXPECT_TRUE(bar);
}

// Sets custom span parameters to a system-initial start-repeat bar line and
// check that it is properly applied to it and to the start-repeat bar lines of staves below.
TEST_F(Engraving_BarlineTests, barline04)
{
    const MutMasterScorePtr score = readMutScore("barline04.mscx"sv);
    ASSERT_TRUE(score);

    score->startCmd(TranslatableString::untranslatable("Engraving barline tests"));
    // 'go' to 5th measure
    Measure* msr = score->firstMeasure();
    for (int i = 0; i < 4; i++) {
        msr = msr->nextMeasure();
    }
    // check span data of measure-initial start-repeat bar line
    Segment* seg = msr->findSegment(SegmentType::StartRepeatBarLine, msr->tick());
    ASSERT_TRUE(seg);

    BarLine* bar = static_cast<BarLine*>(seg->element(FIRST_TRACK_IDX));
    ASSERT_TRUE(bar);

    bar->undoChangeProperty(Pid::BARLINE_SPAN, true);
    bar->undoChangeProperty(Pid::BARLINE_SPAN_FROM, 2);
    bar->undoChangeProperty(Pid::BARLINE_SPAN_TO, 6);
    score->endCmd();

    EXPECT_TRUE(static_cast<bool>(bar->spanStaff()));
    EXPECT_EQ(bar->spanFrom(), 2);
    EXPECT_EQ(bar->spanTo(), 6);

    // check start-repeat bar ine in second staff is gone
    EXPECT_EQ(seg->element(track_idx_t { 1 }), nullptr);
}

// Adds a line break in the middle of a end-start-repeat bar line and then checks the two resulting
// bar lines (an end-repeat and a start-repeat) are not marked as generated.
TEST_F(Engraving_BarlineTests, barline05)
{
    const MutMasterScorePtr score = readMutScore("barline05.mscx"sv);
    ASSERT_TRUE(score);

    // 'go' to 4th measure
    Measure* msr = score->firstMeasure();
    for (int i = 0; i < 3; i++) {
        msr = msr->nextMeasure();
    }
    // create and add a LineBreak element
    LayoutBreak* lb = Factory::createLayoutBreak(msr);
    lb->setLayoutBreakType(LayoutBreakType::LINE);
    lb->setTrack(0);
    lb->setParent(msr);
    score->undoAddElement(lb);
    score->doLayout();

    // check an end-repeat bar line has been created at the end of this measure and it is generated
    Segment* seg = msr->findSegment(SegmentType::EndBarLine, msr->endTick());
    ASSERT_TRUE(seg);

    const BarLine* bar = toBarLine(seg->element(FIRST_TRACK_IDX));
    ASSERT_TRUE(bar);

    EXPECT_EQ(bar->barLineType(), BarLineType::END_REPEAT);
    EXPECT_TRUE(bar->generated());

    // check a start-repeat bar line has been created at the beginning of the next measure and it is generated
    msr = msr->nextMeasure();
    seg = msr->findSegment(SegmentType::StartRepeatBarLine, msr->tick());
    ASSERT_TRUE(seg);

    bar = toBarLine(seg->element(FIRST_TRACK_IDX));
    ASSERT_TRUE(bar);
    EXPECT_TRUE(bar->generated());
}

// Read a score with 3 staves and custom bar line sub-types for staff i-th at measure i-th
// and check the custom syb-types are applied only to their respective bar lines,
// rather than to whole measures.
TEST_F(Engraving_BarlineTests, barline06)
{
    const MasterScorePtr score = readScore("barline06.mscx"sv);
    ASSERT_TRUE(score);

    // scan each measure
    const Measure* msr = score->firstMeasure();
    for (unsigned int i = 0; i < 3; i++) {
        // locate end-measure bar line segment
        const Segment* seg = msr->findSegment(SegmentType::EndBarLine, msr->endTick());
        ASSERT_TRUE(seg);

        // check only i-th staff has custom bar line type
        for (staff_idx_t staffIdx = 0; staffIdx < 3; staffIdx++) {
            const BarLine* bar = toBarLine(seg->element(staff2track(staffIdx)));
            ASSERT_TRUE(bar);

            if (staffIdx != i) {
                // if not the i-th staff, bar should be normal and not custom
                EXPECT_EQ(bar->barLineType(), BarLineType::NORMAL);
            } else {
                // in the i-th staff, the bar line should be of type DOUBLE and custom type should be true
                EXPECT_EQ(bar->barLineType(), BarLineType::DOUBLE);
            }
        }

        msr = msr->nextMeasure();
    }
}

// Drop a normal barline onto measures and barlines of each type of barline
TEST_F(Engraving_BarlineTests, barline179726)
{
    const MutMasterScorePtr score = readMutScore("barline179726.mscx"sv);
    ASSERT_TRUE(score);

    const auto dropNormalBarline = [&](EngravingItem* e) {
        BarLine* barLine = Factory::createBarLine(score->dummy()->segment());
        barLine->setBarLineType(BarLineType::NORMAL);

        EditData dropData{};
        dropData.dropElement = barLine;
        score->startCmd(TranslatableString::untranslatable("Drop normal barline test"));
        e->drop(dropData);
        score->endCmd();
    };

    Measure* m = score->firstMeasure();

    // drop NORMAL onto initial START_REPEAT barline will remove that START_REPEAT
    dropNormalBarline(m->findSegment(SegmentType::StartRepeatBarLine, m->tick())->elementAt(FIRST_TRACK_IDX));
    EXPECT_EQ(m->findSegment(SegmentType::StartRepeatBarLine, m->tick()), nullptr);

    // drop NORMAL onto END_START_REPEAT will turn into NORMAL
    dropNormalBarline(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
    const BarLine* bar = toBarLine(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
    ASSERT_TRUE(bar);
    EXPECT_EQ(bar->barLineType(), BarLineType::NORMAL);

    m = m->nextMeasure();

    // drop NORMAL onto the END_REPEAT part of an END_START_REPEAT straddling a newline will turn into NORMAL
    // at the end of this measure
    dropNormalBarline(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
    bar = toBarLine(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
    ASSERT_TRUE(bar);
    EXPECT_EQ(bar->barLineType(), BarLineType::NORMAL);

    m = m->nextMeasure();

    // but leave START_REPEAT at the beginning of the newline
    bar = toBarLine(m->findSegment(SegmentType::StartRepeatBarLine, m->tick())->elementAt(FIRST_TRACK_IDX));
    EXPECT_TRUE(bar);

    // drop NORMAL onto the measure ending with an END_START_REPEAT straddling a newline will turn into NORMAL
    // at the end of this measure
    // TODO: but note I'm not verifying what happens to the START_REPEAT at the beginning of the newline...
    // I'm not sure that behavior is well-defined yet
    dropNormalBarline(m);
    bar = toBarLine(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
    ASSERT_TRUE(bar);
    EXPECT_EQ(bar->barLineType(), BarLineType::NORMAL);

    m = m->nextMeasure()->nextMeasure();

    // drop NORMAL onto the START_REPEAT part of an END_START_REPEAT straddling a newline will remove the START_REPEAT at the beginning of this measure
    dropNormalBarline(m->findSegment(SegmentType::StartRepeatBarLine, m->tick())->elementAt(FIRST_TRACK_IDX));
    EXPECT_EQ(m->findSegment(SegmentType::StartRepeatBarLine, m->tick()), nullptr);

    // but leave END_REPEAT at the end of previous line
    bar = toBarLine(m->prevMeasure()->findSegment(SegmentType::EndBarLine, m->tick())->elementAt(FIRST_TRACK_IDX));
    ASSERT_TRUE(bar);
    EXPECT_EQ(bar->barLineType(), BarLineType::END_REPEAT);

    for (int i = 0; i < 4; i++) {
        // drop NORMAL onto END_REPEAT, BROKEN, DOTTED, DOUBLE at the end of this meas will turn into NORMAL
        dropNormalBarline(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
        bar = toBarLine(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
        ASSERT_TRUE(bar);
        EXPECT_EQ(bar->barLineType(), BarLineType::NORMAL);
        m = m->nextMeasure();
    }

    m = m->nextMeasure();

    // drop NORMAL onto a START_REPEAT in middle of a line will remove the START_REPEAT at the beginning of this measure
    dropNormalBarline(m->findSegment(SegmentType::StartRepeatBarLine, m->tick())->elementAt(FIRST_TRACK_IDX));
    EXPECT_EQ(m->findSegment(SegmentType::StartRepeatBarLine, m->tick()), nullptr);

    // drop NORMAL onto final END_REPEAT at end of score will turn into NORMAL
    dropNormalBarline(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
    bar = toBarLine(m->findSegment(SegmentType::EndBarLine, m->endTick())->elementAt(FIRST_TRACK_IDX));
    ASSERT_TRUE(bar);
    EXPECT_EQ(bar->barLineType(), BarLineType::NORMAL);
}

TEST_F(Engraving_BarlineTests, deleteSkipBarlines)
{
    const MutMasterScorePtr score = readMutScore("barlinedelete.mscx"sv);
    ASSERT_TRUE(score);

    Measure* m1 = score->firstMeasure();
    ASSERT_TRUE(m1);

    score->startCmd(TranslatableString::untranslatable("Engraving barline tests"));
    score->cmdSelectAll();
    score->cmdDeleteSelection();
    score->endCmd();

    score->doLayout();

    const std::string refFilePath = strings::concat(BARLINE_DATA_DIR, "/barlinedelete-ref.mscx"sv);
    EXPECT_TRUE(ScoreComp::saveCompareScore(score.get(), u"barlinedelete.mscx", String::fromUtf8(refFilePath)));
}
}
