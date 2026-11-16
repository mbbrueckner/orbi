/**
 * @file storage_record.c
 * @brief Implementation of the record format; see storage_record.h.
 */

#include "storage_record.h"

static void put_u16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)(v >> 24);
}

static uint16_t get_u16(const uint8_t* p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_u32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

size_t storage_record_pack(const storage_record_t* record, uint8_t* buf) {
  const pet_state_t* pet = &record->pet;

  buf[0] = STORAGE_RECORD_VERSION;
  buf[1] = pet->hunger;
  buf[2] = pet->energy;
  buf[3] = pet->happiness;
  buf[4] = pet->sleeping ? 1 : 0;
  put_u32(&buf[5], pet->age_seconds);
  put_u16(&buf[9], pet->hunger_carry_s);
  put_u16(&buf[11], pet->energy_carry_s);
  put_u16(&buf[13], pet->happiness_carry_s);
  put_u32(&buf[15], record->last_time_s);
  return STORAGE_RECORD_SIZE;
}

bool storage_record_unpack(const uint8_t* buf,
                           size_t len,
                           storage_record_t* record) {
  if (len != STORAGE_RECORD_SIZE || buf[0] != STORAGE_RECORD_VERSION) {
    return false;
  }
  if (buf[1] > 100 || buf[2] > 100 || buf[3] > 100 || buf[4] > 1) {
    return false;
  }

  pet_state_t* pet = &record->pet;
  pet->hunger = buf[1];
  pet->energy = buf[2];
  pet->happiness = buf[3];
  pet->sleeping = buf[4] != 0;
  pet->age_seconds = get_u32(&buf[5]);
  pet->hunger_carry_s = get_u16(&buf[9]);
  pet->energy_carry_s = get_u16(&buf[11]);
  pet->happiness_carry_s = get_u16(&buf[13]);
  record->last_time_s = get_u32(&buf[15]);
  return true;
}
