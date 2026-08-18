#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// TODO: only implement once an I2C scan has clarified whether a
// CHSC6X or CST816S is fitted (see roadmap step "I2C bus diagnosis")
void input_touch_init(void);

#ifdef __cplusplus
}
#endif
