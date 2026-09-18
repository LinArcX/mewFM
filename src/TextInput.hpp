#pragma once

#include <string>

enum class TextInputResult
{
  None,
  Ok,
  Cancel,
};

struct TextInput
{
  bool active = false;
  std::string label;
  std::string value;
  size_t cursor = 0;
  TextInputResult result = TextInputResult::None;

  void open(const std::string& lbl, const std::string& initial)
  {
    active = true;
    label = lbl;
    value = initial;
    cursor = initial.size();
    result = TextInputResult::None;
  }

  void close()
  {
    active = false;
    result = TextInputResult::None;
  }
};
