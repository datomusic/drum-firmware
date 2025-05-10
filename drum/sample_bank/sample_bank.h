#ifndef SAMPLE_BANK_H_VRABUZT9
#define SAMPLE_BANK_H_VRABUZT9

struct SampleBank {
  constexpr bool get_sample(const unsigned index){
    return index < 32;
  }
};

#endif /* end of include guard: SAMPLE_BANK_H_VRABUZT9 */
