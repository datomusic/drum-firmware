
#include <stdint.h>

struct audio_block_t {
  uint8_t ref_count;
  uint8_t reserved1;
  uint16_t memory_pool_index;
  int16_t data[AUDIO_BLOCK_SAMPLES];
};

class AudioStream {
public:
  AudioStream(unsigned char ninput, audio_block_t **iqueue)
  // : num_inputs(ninput), inputQueue(iqueue)
  {
    /*
    active = false;
    destination_list = NULL;
    for (int i = 0; i < num_inputs; i++) {
      inputQueue[i] = NULL;
    }
    // add to a simple list, for update_all
    // TODO: replace with a proper data flow analysis in update_all
    if (first_update == NULL) {
      first_update = this;
    } else {
      AudioStream *p;
      for (p = first_update; p->next_update; p = p->next_update)
        ;
      p->next_update = this;
    }
    next_update = NULL;
    cpu_cycles = 0;
    cpu_cycles_max = 0;
    numConnections = 0;
    */
  }

  /*
  static void initialize_memory(audio_block_t *data, unsigned int num);
  float processorUsage(void) {
    return CYCLE_COUNTER_APPROX_PERCENT(cpu_cycles);
  }

  float processorUsageMax(void) {
    return CYCLE_COUNTER_APPROX_PERCENT(cpu_cycles_max);
  }

  void processorUsageMaxReset(void) {
    cpu_cycles_max = cpu_cycles;
  }

  bool isActive(void) {
    return active;
  }
  */

  /*
uint16_t cpu_cycles;
uint16_t cpu_cycles_max;
static uint16_t cpu_cycles_total;
static uint16_t cpu_cycles_total_max;
static uint16_t memory_used;
static uint16_t memory_used_max;
  */
protected:
  static void release(audio_block_t *block);
  audio_block_t *receiveReadOnly(unsigned int index = 0);
  audio_block_t *receiveWritable(unsigned int index = 0);
  void transmit(audio_block_t *block, unsigned char index = 0);

static audio_block_t * allocate(void);
  /*
bool active;
unsigned char num_inputs;
static bool update_setup(void);
static void update_stop(void);
static void update_all(void);
friend void software_isr(void);
friend class AudioConnection;
uint8_t numConnections;
  */
private:
  virtual void update(void) = 0;
  /*
static AudioConnection* unused; // linked list of unused but not destructed
connections AudioConnection *destination_list; audio_block_t **inputQueue;
static bool update_scheduled;
static AudioStream *first_update; // for update_all
AudioStream *next_update; // for update_all
static audio_block_t *memory_pool;
static uint32_t memory_pool_available_mask[];
static uint16_t memory_pool_first_mask;
  */
};
