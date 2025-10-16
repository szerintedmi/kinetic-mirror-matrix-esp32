#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>

#ifdef ARDUINO
void setup() {
  UNITY_BEGIN();
  UNITY_END();
}
void loop() {}
#else
int main(int, char**) {
  UNITY_BEGIN();
  return UNITY_END();
}
#endif

