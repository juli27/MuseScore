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

#include <memory>

#include <gtest/gtest.h>

#include "global/io/path.h"

#include "engraving/dom/durationtype.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/mcursor.h"
#include "engraving/types/types.h"

#include "engraving/tests/utils/scorecomp.h"
#include "engraving/tests/utils/scorerw.h"

#include "importexport/midi/internal/midiexport/exportmidi.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::iex::midi;

// forward declaration of private functions used in tests
namespace mu::iex::midi {
extern engraving::Err importMidi(engraving::MasterScore*, const QString& name);
}

static const String MIDI_EXPORT_DATA_DIR("midiexport_data");

class MidiExportTests : public ::testing::Test
{
protected:
    bool saveMidi(Score* score, const QString& name)
    {
        ExportMidi em(score);
        return em.write(name, true, true);
    }

    std::unique_ptr<MasterScore> importMidi(const String& fileName)
    {
        const auto doImportMidi = [](MasterScore* score, const io::path_t& path) -> mu::engraving::Err {
            return ::mu::iex::midi::importMidi(score, path.toQString());
        };

        std::string absPath = std::filesystem::absolute(fileName.toStdString())
                              .generic_string();

        return std::unique_ptr<MasterScore> { ScoreRW::readScore(String::fromStdString(absPath), true, doImportMidi) };
    }
};

//---------------------------------------------------------
///   midi01
///   write/read midi file with timesig 4/4
//---------------------------------------------------------
TEST_F(MidiExportTests, DISABLED_midi01) {
    MCursor c;
    c.setTimeSig(Fraction(4, 4));
    c.createScore(nullptr, u"");
    c.addPart(u"voice");
    c.move(0, Fraction(0, 1));       // move to track 0 tick 0

    c.addKeySig(Key(1));
    c.addTimeSig(Fraction(4, 4));
    c.addChord(60, TDuration(DurationType::V_QUARTER));
    c.addChord(61, TDuration(DurationType::V_QUARTER));
    c.addChord(62, TDuration(DurationType::V_QUARTER));
    c.addChord(63, TDuration(DurationType::V_QUARTER));
    std::unique_ptr<MasterScore> score { c.score() };
    ASSERT_TRUE(score);

    score->doLayout();
    score->rebuildMidiMapping();
    ASSERT_TRUE(ScoreRW::saveScore(score.get(), u"test1a.mscx"));

    ASSERT_TRUE(saveMidi(score.get(), "test1.mid"));

    std::unique_ptr<MasterScore> score2 = importMidi(u"test1.mid");
    ASSERT_TRUE(score2);

    score2->doLayout();
    score2->rebuildMidiMapping();

    EXPECT_TRUE(ScoreComp::saveCompareScore(score2.get(), u"test1b.mscx", MIDI_EXPORT_DATA_DIR + u"/test1a.mscx"));
}
