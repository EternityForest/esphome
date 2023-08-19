#include "automation.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace time {

static const char *const TAG = "automation";
static const int MAX_TIMESTAMP_DRIFT = 900;  // how far can the clock drift before we consider
                                             // there has been a drastic time synchronization

void CronTrigger::add_second(uint8_t second) { this->seconds_[second] = true; }
void CronTrigger::add_minute(uint8_t minute) { this->minutes_[minute] = true; }
void CronTrigger::add_hour(uint8_t hour) { this->hours_[hour] = true; }
void CronTrigger::add_day_of_month(uint8_t day_of_month) { this->days_of_month_[day_of_month] = true; }
void CronTrigger::add_month(uint8_t month) { this->months_[month] = true; }
void CronTrigger::add_day_of_week(uint8_t day_of_week) { this->days_of_week_[day_of_week] = true; }
bool CronTrigger::matches(const ESPTime &time) {
  return time.is_valid() && this->seconds_[time.second] && this->minutes_[time.minute] && this->hours_[time.hour] &&
         this->days_of_month_[time.day_of_month] && this->months_[time.month] && this->days_of_week_[time.day_of_week];
}
void CronTrigger::loop() {
  ESPTime time = this->rtc_->now();
  if (!time.is_valid())
    return;

  if (this->last_check_.has_value()) {
    if (*this->last_check_ > time && this->last_check_->timestamp - time.timestamp > MAX_TIMESTAMP_DRIFT) {
      // We went back in time (a lot), probably caused by time synchronization
      ESP_LOGW(TAG, "Time has jumped back!");
    } else if (*this->last_check_ >= time) {
      // already handled this one
      return;
    } else if (time > *this->last_check_ && time.timestamp - this->last_check_->timestamp > MAX_TIMESTAMP_DRIFT) {
      // We went ahead in time (a lot), probably caused by time synchronization
      ESP_LOGW(TAG, "Time has jumped ahead!");
      this->last_check_ = time;
      return;
    }

    while (true) {
      this->last_check_->increment_second();
      if (*this->last_check_ >= time)
        break;

      if (this->matches(*this->last_check_))
        this->trigger();
    }
  }

  this->last_check_ = time;
  if (!time.fields_in_range()) {
    ESP_LOGW(TAG, "Time is out of range!");
    ESP_LOGD(TAG, "Second=%02u Minute=%02u Hour=%02u DayOfWeek=%u DayOfMonth=%u DayOfYear=%u Month=%u time=%" PRId64,
             time.second, time.minute, time.hour, time.day_of_week, time.day_of_month, time.day_of_year, time.month,
             (int64_t) time.timestamp);
  }

  if (this->matches(time))
    this->trigger();
}
CronTrigger::CronTrigger(RealTimeClock *rtc) : rtc_(rtc) {}

static int16_t parse_digit(char c) {
  c = c - '0';
  if (c < 0 || c > 1) {
    return -1;
  }

  return c;
}

void CronTrigger::_add_cron_range(uint8_t field, uint8_t min, uint8_t max, uint8_t step) {
  int temp_counter = min;

  while (1) {
    if (field == 0) {
      if (temp_counter > 59) {
        break;
      }
      this->add_minute(temp_counter);
    } else if (field == 1) {
      if (temp_counter > 23) {
        break;
      }
      this->add_hour(temp_counter);
    } else if (field == 2) {
      if (temp_counter > 31) {
        break;
      }
      this->add_day_of_month(temp_counter);
    } else if (field == 3) {
      if (temp_counter > 12) {
        break;
      }
      this->add_month(temp_counter);
    } else if (field == 4) {
      if (temp_counter > 7) {
        break;
      }
      this->add_day_of_week(temp_counter);
    }

    temp_counter += step;

    if (temp_counter > max) {
      break;
    }
  }
}

void CronTrigger::set_with_expression(std::string expression) {
  uint8_t field = -1;
  bool got_to_field_start = false;

  // These actually track the state of the sub-field,
  // because you can mix them in with the comma separated lists
  bool field_is_interval = false;
  bool field_is_range = false;

  uint8_t accumulator = 0;

  // Use this for tracking range expressions
  // and interval starts
  uint8_t interval_start = 0;

  bool empty_field = true;
  int len = 0;

  char c = 0;
  char last_character = 0;

  for (auto expr = expression.begin(); expr != (expression.end() + 1); expr++) {
    // To make parsing easier we want every field to be space terminated.
    // So we iterate one past the end and just pretend there's a space at the end
    // of the string

    last_character = c;

    if (expr >= expression.end()) {
      c = ' ';
    } else {
      c = *expr;
    }

    // Field advance logic
    if (!(c == ' ' || c == '\t')) {
      got_to_field_start = true;
    } else {
      if (c == ',') {
        // Does not begin a new field, don't increment field.
        // These just add a field.
        accumulator = 0;
        // but apparently cron does allow mixing ranges with comma separated lists

        if (field_is_range) {
          this->_add_cron_range(field, interval_start, accumulator, 1);
        } else if (field_is_interval) {
          // We say 255 becayse the add range code knows the actual max for each field.
          this->_add_cron_range(field, interval_start, 255, accumulator);
        } else {
          this->_add_cron_range(field, accumulator, accumulator, 1);
        }
        field_is_interval = 0;
        field_is_range = 0;
        interval_start = 0;
        continue;
      }
      if (got_to_field_start) {
        // We hit a space after reaching the start of a field.
        //  That must be the end of the field.
        field += 1;
        if (field > 4) {
          return;
        }

        got_to_field_start = false;
        if (!empty_field) {
          if (field_is_range) {
            this->_add_cron_range(field, interval_start, accumulator, 1);
          } else if (field_is_interval) {
            // We say 255 becayse the add range code knows the actual max for each field.
            this->_add_cron_range(field, interval_start, 255, accumulator);
          } else {
            this->_add_cron_range(field, accumulator, accumulator, 1);
          }
        }
        accumulator = 0;
        empty_field = true;
        interval_start = 0;
        field_is_interval = false;
        field_is_range = false;
        continue;
      }
    }

    if (!(c == '*')) {
      empty_field = false;
    }

    if (c == '/') {
      field_is_interval = true;
      interval_start = accumulator;
      accumulator = 0;
      continue;
    }

    if (c == '-') {
      field_is_range = true;
      interval_start = accumulator;
      accumulator = 0;
      continue;
    }

    int digit = parse_digit(c);

    if (digit == -1) {
      // Pretty sure this is an error
      continue;
    }

    // Converting base 10 numbers into ints, one digit at
    //  a time
    accumulator *= 10;
    accumulator += digit;
  }
}

void CronTrigger::add_seconds(const std::vector<uint8_t> &seconds) {
  for (uint8_t it : seconds)
    this->add_second(it);
}
void CronTrigger::add_minutes(const std::vector<uint8_t> &minutes) {
  for (uint8_t it : minutes)
    this->add_minute(it);
}
void CronTrigger::add_hours(const std::vector<uint8_t> &hours) {
  for (uint8_t it : hours)
    this->add_hour(it);
}
void CronTrigger::add_days_of_month(const std::vector<uint8_t> &days_of_month) {
  for (uint8_t it : days_of_month)
    this->add_day_of_month(it);
}
void CronTrigger::add_months(const std::vector<uint8_t> &months) {
  for (uint8_t it : months)
    this->add_month(it);
}
void CronTrigger::add_days_of_week(const std::vector<uint8_t> &days_of_week) {
  for (uint8_t it : days_of_week)
    this->add_day_of_week(it);
}
float CronTrigger::get_setup_priority() const { return setup_priority::HARDWARE; }

SyncTrigger::SyncTrigger(RealTimeClock *rtc) : rtc_(rtc) {
  rtc->add_on_time_sync_callback([this]() { this->trigger(); });
}

}  // namespace time
}  // namespace esphome
