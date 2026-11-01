/**
 * @file Modal.hpp
 * @brief Declares ModalType, ModalResult, and the Modal dialog state struct.
 */

#pragma once

#include <string>
#include <vector>

/**
 * @brief Enumerates the kinds of modal dialogs the application can show.
 */
enum class ModalType
{
  None,     ///< No modal is active.
  Info,     ///< Informational modal with a single OK button.
  Confirm,  ///< Confirmation modal with Yes/No buttons.
  OpenWith, ///< Application chooser with a scrollable list.
  Settings, ///< Application settings modal.
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
  Pick, ///< The user picked an item from a list.
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
  ModalResult result = ModalResult::None;  ///< Latest result chosen by the user.
  int focus = -1;                          ///< Index of the focused button (-1 = none).
  std::string openWithPath;                ///< Path of the file being opened.
  std::vector<std::string> openWithLabels; ///< Display names of candidate applications.
  std::vector<std::string> openWithExecs;  ///< Exec commands matching openWithLabels.
  int openWithSelected = -1;                   ///< Highlighted item (-1 = none).
  int openWithScroll = 0;                      ///< Index of the first visible item.
  bool openWithScrollToSelected = true;        ///< Auto-scroll to keep selection visible.
  bool openWithScrollToCenter = false;         ///< Auto-scroll to center the initial selection.
  std::vector<std::string> openWithDesktopIds; ///< Desktop file IDs of candidate applications.
  std::string openWithMime;                    ///< MIME type of the file being opened.
  std::string openWithDefaultId;               ///< Desktop file ID of the default app.
  std::string openWithFilter;                  ///< Search filter string.
  std::vector<std::string> availFonts = {"Default (Embedded)"}; ///< Paths to system fonts.
  size_t fontIdx = 0;                          ///< Index of the selected font.
  std::vector<std::string> availSizes = {"10", "12", "14", "16", "18", "20", "24", "28", "32"}; ///< Selectable font sizes.
  size_t sizeIdx = 3;                          ///< Index of the selected font size.
  int settingsDropdown = -1;                   ///< Which Settings dropdown is open (-1 = none).
  float settingsScroll = 0.0f;                 ///< Scroll offset for the font dropdown.
  std::string settingsFilter;                  ///< Search filter for the font dropdown.
  int settingsSelectedItem = -1;               ///< Highlighted item in the font dropdown.
  bool applyFontPending = false;               ///< Set to true when the user selects a font.

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
   * @brief Opens the settings modal.
   */
  void openSettings()
  {
    active = true;
    type = ModalType::Settings;
    title = "Settings";
    message = "Settings will be implemented here.";
    result = ModalResult::None;
    focus = 0;
  }

  /**
   * @brief Opens the Open With chooser dialog.
   * @param path   Absolute path of the file to open.
   * @param labels Display names of the candidate applications.
   * @param execs  Exec commands matching @p labels.
   */
  void openOpenWith(const std::string& path,
                    const std::vector<std::string>& labels,
                    const std::vector<std::string>& execs,
                    const std::vector<std::string>& desktopIds,
                    const std::string& mime,
                    const std::string& defaultId)
  {
    active = true;
    type = ModalType::OpenWith;
    title = "Open With";
    message.clear();
    result = ModalResult::None;
    focus = 2;
    openWithPath = path;
    openWithLabels = labels;
    openWithExecs = execs;
    openWithDesktopIds = desktopIds;
    openWithMime = mime;
    openWithDefaultId = defaultId;
    openWithFilter.clear();
    openWithSelected = labels.empty() ? -1 : 0;
    for (size_t i = 0; i < desktopIds.size(); i++)
    {
      if (!defaultId.empty() && desktopIds[i] == defaultId)
      {
        openWithSelected = static_cast<int>(i);
        break;
      }
    }
    openWithScroll = 0;
    openWithScrollToSelected = true;
    openWithScrollToCenter = true;
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
