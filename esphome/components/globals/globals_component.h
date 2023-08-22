#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include <cstring>


// This is a legacy compatibility hack, which seems
// less hacky than any other way I can think to do it.
// We normally let the YAML configure whether to use flash, in this case we
// also need to respect the old way of setting flash prefs.

#ifdef USE_ESP8266_PREFERENCES_FLASH
static const bool force_legacy_flash_ = true;
#else
static const bool force_legacy_flash_ = false;
#endif

namespace esphome {
namespace globals {

template<typename T> class GlobalsComponent : public Component {
 public:
  using value_type = T;
  explicit GlobalsComponent() = default;
  explicit GlobalsComponent(T initial_value) : value_(initial_value) {}
  explicit GlobalsComponent(std::array<typename std::remove_extent<T>::type, std::extent<T>::value> initial_value) {
    memcpy(this->value_, initial_value.data(), sizeof(T));
  }

  T &value() { return this->value_; }
  void setup() override {}

 protected:
  T value_{};
};

template<typename T, bool IN_FLASH> class RestoringGlobalsComponent : public Component {
 public:
  using value_type = T;
  explicit RestoringGlobalsComponent() = default;
  explicit RestoringGlobalsComponent(T initial_value) : value_(initial_value) {}
  explicit RestoringGlobalsComponent(
      std::array<typename std::remove_extent<T>::type, std::extent<T>::value> initial_value) {
    memcpy(this->value_, initial_value.data(), sizeof(T));
  }

  T &value() { return this->value_; }

  void setup() override {
    this->rtc_ = global_preferences->make_preference<T>(1944399030U ^ this->name_hash_, IN_FLASH || force_legacy_flash_);
    this->rtc_.load(&this->value_);
    memcpy(&this->prev_value_, &this->value_, sizeof(T));
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void loop() override { store_value_(); }

  void on_shutdown() override { store_value_(); }

  void set_name_hash(uint32_t name_hash) { this->name_hash_ = name_hash; }

 protected:
  void store_value_() {
    int diff = memcmp(&this->value_, &this->prev_value_, sizeof(T));
    if (diff != 0) {
      this->rtc_.save(&this->value_);
      memcpy(&this->prev_value_, &this->value_, sizeof(T));
    }
  }

  T value_{};
  T prev_value_{};
  uint32_t name_hash_{};
  ESPPreferenceObject rtc_;
};

/// Use with string or subclasses of strings
template<typename T, uint8_t SZ, bool IN_FLASH> class RestoringGlobalStringComponent : public Component {
 public:
  using value_type = T;
  explicit RestoringGlobalStringComponent() = default;
  explicit RestoringGlobalStringComponent(T initial_value) { value_ = initial_value; }

  T &value() { return this->value_; }

  void setup() override {
    char temp[SZ];
    this->rtc_ = global_preferences->make_preference<uint8_t[SZ]>(1944399030U ^ this->name_hash_, IN_FLASH || force_legacy_flash_);
    bool hasdata = this->rtc_.load(&temp);
    if (hasdata) {
      this->value_.assign(temp + 1, temp[0]);
    }
    this->prev_value_.assign(this->value_);

  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void loop() override { store_value_(); }

  void on_shutdown() override { store_value_(); }

  void set_name_hash(uint32_t name_hash) { this->name_hash_ = name_hash; }

 protected:
  void store_value_() {
    int diff = this->value_.compare(this->prev_value_);

    if (diff != 0) {
      // Make it into a length prefixed thing
      char temp[SZ];
      memcpy(temp + 1, this->value_.c_str(), min((int) this->value_.size(), SZ - 1));
      temp[0] = (char) (min((int)this->value_.size(), SZ-1));
      this->rtc_.save(&temp);
      this->prev_value_.assign(this->value_);
    }


  }

  T value_{};
  T prev_value_{};
  uint32_t name_hash_{};
  ESPPreferenceObject rtc_;
};

template<class C, typename... Ts> class GlobalVarSetAction : public Action<Ts...> {
 public:
  explicit GlobalVarSetAction(C *parent) : parent_(parent) {}

  using T = typename C::value_type;

  TEMPLATABLE_VALUE(T, value);

  void play(Ts... x) override { this->parent_->value() = this->value_.value(x...); }

 protected:
  C *parent_;
};

template<typename T> T &id(GlobalsComponent<T> *value) { return value->value(); }
template<typename T, bool IN_FLASH> T &id(RestoringGlobalsComponent<T,IN_FLASH> *value) { return value->value(); }
template<typename T, uint8_t SZ, bool IN_FLASH> T &id(RestoringGlobalStringComponent<T, SZ, IN_FLASH> *value) { return value->value(); }

}  // namespace globals
}  // namespace esphome
