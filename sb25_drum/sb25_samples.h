#include "etl/array.h"
#include "sample_data.h"
struct SampleData {
  const int16_t *data;
  const uint32_t length;
};
static constexpr const etl::array<SampleData, 32> all_samples = {
    SampleData{samples_Kick_C78__pcm.begin(), samples_Kick_C78__pcm.size()},
    SampleData{samples_skkick__pcm.begin(), samples_skkick__pcm.size()},
    SampleData{samples_JR_SDD_KICK_1_1__pcm.begin(), samples_JR_SDD_KICK_1_1__pcm.size()},
    SampleData{samples_nes_13_01__pcm.begin(), samples_nes_13_01__pcm.size()},
    SampleData{samples_26887__vexst__kick_3_1__pcm.begin(),
               samples_26887__vexst__kick_3_1__pcm.size()},
    SampleData{samples_006__pcm.begin(), samples_006__pcm.size()},
    SampleData{samples_duo_kick_01__pcm.begin(), samples_duo_kick_01__pcm.size()},
    SampleData{samples_Kick_909_23__pcm.begin(), samples_Kick_909_23__pcm.size()},
    SampleData{samples_JR_SDD_SNARE_10__pcm.begin(), samples_JR_SDD_SNARE_10__pcm.size()},
    SampleData{samples_sksnare__pcm.begin(), samples_sksnare__pcm.size()},
    SampleData{samples_Snare_C78_with_silence__pcm.begin(),
               samples_Snare_C78_with_silence__pcm.size()},
    SampleData{samples_nes_09_01__pcm.begin(), samples_nes_09_01__pcm.size()},
    SampleData{samples_26901__vexst__snare_2_1__pcm.begin(),
               samples_26901__vexst__snare_2_1__pcm.size()},
    SampleData{samples_015__pcm.begin(), samples_015__pcm.size()},
    SampleData{samples_FR_BB_Sarik_Snare_004_1__pcm.begin(),
               samples_FR_BB_Sarik_Snare_004_1__pcm.size()},
    SampleData{samples_Snare_909_3__pcm.begin(), samples_Snare_909_3__pcm.size()},
    SampleData{samples_DR110_clap__pcm.begin(), samples_DR110_clap__pcm.size()},
    SampleData{samples_cowbell_hi__pcm.begin(), samples_cowbell_hi__pcm.size()},
    SampleData{samples_JR_SDD_SNARE_RIM_1__pcm.begin(), samples_JR_SDD_SNARE_RIM_1__pcm.size()},
    SampleData{samples_DR55RIM__pcm.begin(), samples_DR55RIM__pcm.size()},
    SampleData{samples_Zap_2__pcm.begin(), samples_Zap_2__pcm.size()},
    SampleData{samples_Finger_Snap__pcm.begin(), samples_Finger_Snap__pcm.size()},
    SampleData{samples_44_Analog_Cowbell__pcm.begin(), samples_44_Analog_Cowbell__pcm.size()},
    SampleData{samples_vocal_3__pcm.begin(), samples_vocal_3__pcm.size()},
    SampleData{samples_JR_SDD_HAT_A1_mono__pcm.begin(), samples_JR_SDD_HAT_A1_mono__pcm.size()},
    SampleData{samples_skclhat__pcm.begin(), samples_skclhat__pcm.size()},
    SampleData{samples_DR55HAT__pcm.begin(), samples_DR55HAT__pcm.size()},
    SampleData{samples_nes_09_01__pcm.begin(),
               samples_nes_09_01__pcm.size()}, // Note: duplicate of the 12th sample
    SampleData{samples_26880__vexst__closed_hi_hat_2_1__pcm.begin(),
               samples_26880__vexst__closed_hi_hat_2_1__pcm.size()},
    SampleData{samples_duo_hat_01__pcm.begin(), samples_duo_hat_01__pcm.size()},
    SampleData{samples_FR_BB_Sarik_HHat_010_1__pcm.begin(),
               samples_FR_BB_Sarik_HHat_010_1__pcm.size()},
    SampleData{samples_ClosedHH_909X_2__pcm.begin(), samples_ClosedHH_909X_2__pcm.size()},
};