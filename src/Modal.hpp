#pragma once

#include <string>

enum class ModalType
{
  None,
  Info,
  Confirm,
};

enum class ModalResult
{
  None,
  Ok,
  Yes,
  No,
};

struct Modal
{
  bool active = false;
  ModalType type = ModalType::None;
  std::string title;
  std::string message;
  ModalResult result = ModalResult::None;

  void openInfo(const std::string& t, const std::string& msg)
  {
    active = true;
    type = ModalType::Info;
    title = t;
    message = msg;
    result = ModalResult::None;
  }

  void openConfirm(const std::string& t, const std::string& msg)
  {
    active = true;
    type = ModalType::Confirm;
    title = t;
    message = msg;
    result = ModalResult::None;
  }

  void close()
  {
    active = false;
    type = ModalType::None;
    result = ModalResult::None;
  }
};
