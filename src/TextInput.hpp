#pragma once

#include <string>

/**
 * @brief Enumerates the possible outcomes of a text input dialog.
 */
enum class TextInputResult
{
  None,   ///< The dialog is still open (no result yet).
  Ok,     ///< The user confirmed with OK.
  Cancel, ///< The user cancelled the dialog.
};

/**
 * @brief State and helper operations for a single-line text input dialog.
 *
 * Used for operations such as New Folder, New File, Rename, Go to Path, and
 * live Filter. The struct stores the current text, cursor position, and the
 * last result produced by the dialog.
 */
struct TextInput
{
  bool active = false;                            ///< True while the dialog is visible.
  std::string label;                              ///< Label shown above the input field.
  std::string value;                              ///< Current text content of the field.
  size_t cursor = 0;                              ///< Cursor position (index into value).
  TextInputResult result = TextInputResult::None; ///< Latest result chosen by the user.

  /**
   * @brief Opens the dialog with an initial value.
   * @param lbl     Label to show above the field.
   * @param initial Initial text to place in the field.
   */
  void open(const std::string& lbl, const std::string& initial)
  {
    active = true;
    label = lbl;
    value = initial;
    cursor = initial.size();
    result = TextInputResult::None;
  }

  /**
   * @brief Closes the dialog and clears any pending result.
   */
  void close()
  {
    active = false;
    result = TextInputResult::None;
  }
};
