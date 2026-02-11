/**
 * @file flash_safety_override.c
 * @brief Custom flash safety helper that keeps the audio DMA IRQ enabled
 *        during flash erase/program operations.
 *
 * The default pico_flash safety helper disables ALL interrupts (via PRIMASK)
 * during flash operations. This causes audio underruns because the DMA ISR
 * cannot service audio buffers during the ~50ms sector erase window.
 *
 * This override uses NVIC-level interrupt masking instead of PRIMASK, which
 * allows selectively keeping the audio DMA IRQ enabled while disabling all
 * other interrupts. This is safe because:
 *   - The audio DMA ISR and its call chain run from RAM
 *   - The ISR only reads from pre-filled RAM buffers (producer pool)
 *   - All other interrupts remain disabled for safety
 *
 * PRECONDITIONS:
 *   - audio_i2s_dma_irq_handler must be __time_critical_func (RAM-resident)
 *   - Audio buffer pool functions called from the ISR must be in RAM
 *   - The FlashAccessCoordinator must have pre-buffered audio data before
 *     any flash write is initiated
 */

#include "hardware/irq.h"
#include "hardware/structs/nvic.h"
#include "pico/audio_i2s.h"
#include "pico/flash.h"

/* RP2350 has 52 IRQs, requiring 2 NVIC ISER/ICER registers (0-31, 32-51) */
#define NVIC_REG_COUNT 2

/* Saved NVIC interrupt-enable state for restore after flash operation */
static uint32_t saved_nvic_iser[NVIC_REG_COUNT];

static bool __not_in_flash_func(selective_init_deinit)(bool init) {
  (void)init;
  return true;
}

/**
 * Enter the flash-safe zone by disabling all NVIC interrupts except the
 * audio DMA IRQ. Uses direct NVIC register writes (not irq_set_enabled)
 * because irq_set_enabled itself may reside in flash.
 */
static int __not_in_flash_func(selective_enter_safe_zone)(uint32_t timeout_ms) {
  (void)timeout_ms;

  /* The audio DMA IRQ number: DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ
   * On RP2350: DMA_IRQ_0 = 10, PICO_AUDIO_I2S_DMA_IRQ is typically 0
   * So audio_irq = 10, which is in NVIC register 0 (bit 10) */
  const uint audio_irq = DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ;
  const uint audio_irq_reg = audio_irq / 32;
  const uint32_t audio_irq_bit = 1u << (audio_irq % 32);

  /* 1. Save current NVIC enable state */
  for (int i = 0; i < NVIC_REG_COUNT; i++) {
    saved_nvic_iser[i] = nvic_hw->iser[i];
  }

  /* 2. Disable ALL interrupts at NVIC level */
  for (int i = 0; i < NVIC_REG_COUNT; i++) {
    nvic_hw->icer[i] = 0xFFFFFFFF;
  }

  /* 3. Re-enable ONLY the audio DMA IRQ */
  nvic_hw->icpr[audio_irq_reg] = audio_irq_bit; /* Clear any pending */
  nvic_hw->iser[audio_irq_reg] = audio_irq_bit; /* Enable */

  return PICO_OK;
}

/**
 * Exit the flash-safe zone by restoring the original NVIC interrupt state.
 */
static int __not_in_flash_func(selective_exit_safe_zone)(uint32_t timeout_ms) {
  (void)timeout_ms;

  /* Restore original NVIC enable state:
   * - First disable everything that wasn't originally enabled
   * - Then enable everything that was originally enabled */
  for (int i = 0; i < NVIC_REG_COUNT; i++) {
    nvic_hw->icer[i] = ~saved_nvic_iser[i];
    nvic_hw->iser[i] = saved_nvic_iser[i];
  }

  return PICO_OK;
}

static flash_safety_helper_t selective_helper = {
    .core_init_deinit = selective_init_deinit,
    .enter_safe_zone_timeout_ms = selective_enter_safe_zone,
    .exit_safe_zone_timeout_ms = selective_exit_safe_zone,
};

/**
 * Strong override of the SDK's weak get_flash_safety_helper().
 * Returns our selective NVIC masking helper instead of the default
 * PRIMASK-based helper.
 */
flash_safety_helper_t *get_flash_safety_helper(void) {
  return &selective_helper;
}
