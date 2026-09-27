#pragma once

#include <string>

/**
 * @brief Enumerates the kinds of modal dialogs the application can show.
 */
enum class ModalType
{
  None,    ///< No modal is active.
  Info,    ///< Informational modal with a single OK button.
  Confirm, ///< Confirmation modal with Yes/No buttons.
};

/**
 * @brief Enumerates the possible results returned by a modal dialog.
 */
enum class ModalResult
{
  None, ///< The modal has not produced a result yet.
  Ok,   ///< The user pressed the OK button.
  Yes,  ///< The user confirmed with Yes.
  No,   ///< The user declined with No.
};

/**
 * @brief State and helper operations for a simple modal dialog.
 *
 * Stores the currently displayed modal's title and message, the button
 * the user has focused, and the latest result the user produced. The UI
 * layer renders the modal based on this state and calls openInfo(),
 * openConfirm(), and close() to drive the dialog lifecycle.
 */
struct Modal
{
  bool active = false;                    ///< True while the modal is visible.
  ModalType type = ModalType::None;       ///< Kind of modal currently being shown.
  std::string title;                      ///< Window title text.
  std::string message;                    ///< Body text shown to the user.
  ModalResult result = ModalResult::None; ///< Latest result chosen by the user.
  int focus = -1;                         ///< Index of the focused button (-1 = none).

  /**
   * @brief Opens an informational modal with a single OK button.
   * @param t   Title of the modal.
   * @param msg Body text of the modal.
   */
  void openInfo(const std::string& t, const std::string& msg)
  {
    active = true;
    type = ModalType::Info;
    title = t;
    message = msg;
    result = ModalResult::None;
    focus = 0;
  }

  /**
   * @brief Opens a confirmation modal with Yes/No buttons.
   * @param t   Title of the modal.
   * @param msg Body text of the modal.
   */
  void openConfirm(const std::string& t, const std::string& msg)
  {
    active = true;
    type = ModalType::Confirm;
    title = t;
    message = msg;
    result = ModalResult::None;
    focus = 0;
  }

  /**
   * @brief Closes the modal and clears any pending result.
   */
  void close()
  {
    active = false;
    type = ModalType::None;
    result = ModalResult::None;
  }
};
