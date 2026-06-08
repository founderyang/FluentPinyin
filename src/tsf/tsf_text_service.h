#pragma once

#include "common/rime_types.h"
#include "tsf/rime_core_client.h"

#include <msctf.h>
#include <windows.h>

#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace fp::tsf {

class TsfTextService final : public ITfTextInputProcessorEx,
                             public ITfKeyEventSink,
                             public ITfCompositionSink,
                             public ITfDisplayAttributeProvider {
 public:
  TsfTextService();
  TsfTextService(const TsfTextService&) = delete;
  TsfTextService& operator=(const TsfTextService&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  STDMETHODIMP Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) override;
  STDMETHODIMP ActivateEx(ITfThreadMgr* thread_mgr,
                          TfClientId client_id,
                          DWORD flags) override;
  STDMETHODIMP Deactivate() override;

  STDMETHODIMP OnSetFocus(BOOL foreground) override;
  STDMETHODIMP OnTestKeyDown(ITfContext* context,
                             WPARAM wparam,
                             LPARAM lparam,
                             BOOL* eaten) override;
  STDMETHODIMP OnKeyDown(ITfContext* context,
                         WPARAM wparam,
                         LPARAM lparam,
                         BOOL* eaten) override;
  STDMETHODIMP OnTestKeyUp(ITfContext* context,
                           WPARAM wparam,
                           LPARAM lparam,
                           BOOL* eaten) override;
  STDMETHODIMP OnKeyUp(ITfContext* context,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL* eaten) override;
  STDMETHODIMP OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;
  STDMETHODIMP OnCompositionTerminated(TfEditCookie edit_cookie,
                                       ITfComposition* composition) override;
  STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** enum_info) override;
  STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid,
                                       ITfDisplayAttributeInfo** info) override;

  void HandleLangBarMenuCommand(UINT command_id);
  void ShowContextMenu(POINT point);
  void ToggleToolbarWindow();
  void ToggleAsciiMode();
  [[nodiscard]] bool toolbar_visible() const;
  [[nodiscard]] bool full_shape_mode() const noexcept { return full_shape_mode_; }
  [[nodiscard]] bool chinese_punctuation_mode() const noexcept { return chinese_punctuation_mode_; }
  [[nodiscard]] bool simplified_charset() const noexcept { return simplified_charset_; }
  [[nodiscard]] bool ascii_mode() const noexcept { return ascii_mode_; }
  [[nodiscard]] bool has_input_focus() const noexcept { return has_input_focus_; }
  [[nodiscard]] bool status_tip_enabled() const noexcept { return status_tip_enabled_; }
  [[nodiscard]] std::wstring_view theme_mode() const noexcept { return theme_mode_; }
  [[nodiscard]] std::wstring_view theme_preset() const noexcept { return theme_preset_; }
  [[nodiscard]] bool horizontal_candidate_layout() const noexcept {
    return horizontal_candidate_layout_;
  }
  [[nodiscard]] int compact_candidate_count() const noexcept {
    return compact_candidate_count_;
  }

 private:
  ~TsfTextService();

  bool IsComposing() const noexcept;
  bool IsModifierShortcutActive() const;
  bool IsKeyHandled(WPARAM wparam, LPARAM lparam) const;
  bool HandleKey(ITfContext* context, WPARAM wparam, LPARAM lparam);
  bool ToggleAsciiModeFromKey(ITfContext* context, bool caps_lock);
  bool SyncCapsLockAsciiMode(ITfContext* context);
  bool IsRimeReady() const;
  bool EnsureRimeReadyForKey();
  void WarmUpRimeAsync(DWORD delay_ms = 0);
  void InitializeRime();
  void UninitializeRime();
  void ApplyRimeOptions();
  void ApplyRimeOptionsLocked();
  void PersistInputModeDefaults() const;
  void RefreshCandidates();
  void ChangeCandidatePage(ITfContext* context, int delta);
  [[nodiscard]] size_t VisibleCandidateCount() const;
  [[nodiscard]] int CandidateNavigationColumns() const;
  [[nodiscard]] size_t CandidateIndexForDigit(size_t digit_index) const;
  void ClampCandidateSelection();
  bool MoveCandidateSelection(WPARAM wparam);
  bool SetCandidateExpansion(ITfContext* context, bool expanded);
  void NotifyInputModeChanged();
  void UpdateInputModeCompartments();
  void LoadUserSettings(bool force = false, bool allow_candidate_changes_during_composition = false);
  void ApplyInputStateFromSettings(bool allow_during_composition);
  void ApplyDefaultInputStateFromSettings();
  void RefreshInputStateFromSettings();
  void RefreshCandidateWindowVisualSettings();
  void ReloadCandidateWindowVisualSettings();
  void SaveCandidateLayoutSetting() const;
  void SaveToolbarSetting() const;
  [[nodiscard]] int CandidateFontPointSize() const noexcept;
  void CreateControlWindow();
  void DestroyControlWindow();
  ATOM EnsureControlWindowClass();
  LRESULT ControlWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticControlWindowProc(HWND window,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam);
  bool UpdatePreedit(ITfContext* context, const std::wstring& text);
  bool CommitText(ITfContext* context, const std::wstring& text);
  bool CommitTextAndContinueComposition(ITfContext* context,
                                        const std::wstring& commit_text,
                                        const std::string& remaining_input,
                                        const std::wstring& preedit_text);
  bool CommitCandidate(ITfContext* context, size_t candidate_index);
  bool CommitCandidateFromMouse(size_t candidate_index);
  bool CancelComposition(ITfContext* context);
  void ClearCompositionState();
  void ShowCandidateWindow(ITfContext* context = nullptr);
  void HideCandidateWindow();
  void DestroyCandidateWindow();
  void OpenCandidateSettingsMenu();
  bool ShouldKeepCandidateWindow() const;
  POINT CandidateWindowAnchor(ITfContext* context);
  POINT CandidateWindowAnchorFromContext(ITfContext* context);
  void RememberCandidateAnchor(POINT anchor);
  void DrawCandidateWindow(HDC dc);
  void RenderCandidateLayeredWindow();
  ATOM EnsureCandidateWindowClass();
  LRESULT CandidateWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticCandidateWindowProc(HWND window,
                                                    UINT message,
                                                    WPARAM wparam,
                                                    LPARAM lparam);
  void ShowCandidateTooltip(int tool);
  void HideCandidateTooltip();
  void PositionCandidateTooltip(int tool);
  void DrawCandidateTooltip(HDC dc);
  void RenderCandidateTooltipLayeredWindow();
  ATOM EnsureCandidateTooltipWindowClass();
  LRESULT CandidateTooltipWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticCandidateTooltipWindowProc(HWND window,
                                                           UINT message,
                                                           WPARAM wparam,
                                                           LPARAM lparam);
  enum class StatusTipDetail {
    kNone,
    kChinesePunctuation,
    kEnglishPunctuation,
    kFullShape,
    kHalfShape,
  };
  enum class StatusTipIconMode {
    kChinesePunctuation,
    kEnglishPunctuation,
    kFullShape,
    kHalfShape,
  };
  void ShowStatusTip(ITfContext* context = nullptr,
                     StatusTipDetail detail = StatusTipDetail::kNone);
  void HideStatusTip();
  void DestroyStatusTip();
  void PositionStatusTip(ITfContext* context);
  void DrawStatusTip(HDC dc);
  void RenderStatusTipLayeredWindow();
  bool IsStatusTipAllowed(ITfContext* context) const;
  std::wstring StatusTipText() const;
  ATOM EnsureStatusTipWindowClass();
  LRESULT StatusTipWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticStatusTipWindowProc(HWND window,
                                                    UINT message,
                                                    WPARAM wparam,
                                                    LPARAM lparam);
  void ShowToolbarWindow();
  void HideToolbarWindow();
  void DestroyToolbarWindow();
  void RefreshToolbarFromSettings();
  bool ShouldHostToolbarWindow() const;
  bool TryAcquireToolbarWindowOwnership();
  void ReleaseToolbarWindowOwnership();
  void KeepToolbarWindowTopmost();
  void EnsureToolbarWindowTopmost();
  void PositionToolbarWindow(bool show_window = true);
  void PositionToolbarWindowKeepingCenter(bool show_window = true);
  void DrawToolbarWindow(HDC dc);
  void RenderToolbarLayeredWindow();
  void ShowToolbarGearMenu(POINT point);
  void SaveToolbarItemsSetting() const;
  void ToggleToolbarItemVisibility(int item);
  void ToggleToolbarLayout();
  void ShowToolbarTooltip(int item);
  void HideToolbarTooltip();
  void DestroyToolbarTooltip();
  void PositionToolbarTooltip(int item);
  void DrawToolbarTooltip(HDC dc);
  void RenderToolbarTooltipLayeredWindow();
  ATOM EnsureToolbarTooltipWindowClass();
  LRESULT ToolbarTooltipWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticToolbarTooltipWindowProc(HWND window,
                                                         UINT message,
                                                         WPARAM wparam,
                                                         LPARAM lparam);
  bool IsCurrentKeyboardProfile() const;
  ATOM EnsureToolbarWindowClass();
  LRESULT ToolbarWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticToolbarWindowProc(HWND window,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam);
  void ShowContextMenu(POINT point, bool toolbar_mode);
  void HideContextMenu();
  void DestroyContextMenu();
  void DrawContextMenu(HDC dc);
  void RenderContextMenuLayeredWindow();
  ATOM EnsureContextMenuWindowClass();
  LRESULT ContextMenuWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticContextMenuWindowProc(HWND window,
                                                      UINT message,
                                                      WPARAM wparam,
                                                      LPARAM lparam);
  void ShowContextSubmenu(int parent_row);
  void HideContextSubmenu();
  void DestroyContextSubmenu();
  void DrawContextSubmenu(HDC dc);
  void RenderContextSubmenuLayeredWindow();
  ATOM EnsureContextSubmenuWindowClass();
  LRESULT ContextSubmenuWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticContextSubmenuWindowProc(HWND window,
                                                         UINT message,
                                                         WPARAM wparam,
                                                         LPARAM lparam);
  void OpenConfigApp(const wchar_t* page = nullptr);
  void OpenRimeUserDirectory();
  void RedeployRime();
  void RestartRimeAndAlgorithmServiceAsync();
  void ShutdownRimeForUninstall();
  void ReloadRimeAndAlgorithmService();
  void RestartRimeAndAlgorithmService();
  void OpenEmojiPanel();
  void OpenFluentPinyinWebsite();

  std::atomic<unsigned long> ref_count_{1};
  ITfThreadMgr* thread_mgr_ = nullptr;
  ITfKeystrokeMgr* keystroke_mgr_ = nullptr;
  ITfLangBarItemMgr* lang_bar_item_mgr_ = nullptr;
  ITfLangBarItemButton* brand_item_ = nullptr;
  ITfLangBarItemButton* input_mode_item_ = nullptr;
  TfClientId client_id_ = 0;
  TfGuidAtom display_attribute_input_atom_ = TF_INVALID_GUIDATOM;
  std::unique_ptr<RimeCoreClient> rime_;
  mutable std::mutex rime_mutex_;
  bool service_active_ = false;
  bool rime_ready_ = false;
  bool rime_warmup_requested_ = false;
  bool rime_restart_requested_ = false;
  std::string composition_input_;
  LONG preedit_length_ = 0;
  int candidate_page_index_ = 0;
  bool has_previous_candidate_page_ = false;
  bool has_next_candidate_page_ = false;
  std::vector<fp::core::RimeCandidateView> candidates_;
  size_t selected_candidate_index_ = 0;
  std::string last_candidate_query_input_;
  int last_candidate_query_page_index_ = -1;
  int last_candidate_query_page_size_ = 0;
  ITfComposition* composition_ = nullptr;
  HWND candidate_window_ = nullptr;
  HWND candidate_tooltip_window_ = nullptr;
  int candidate_tooltip_tool_ = 0;
  HWND status_tip_window_ = nullptr;
  HWND toolbar_window_ = nullptr;
  HANDLE toolbar_owner_mutex_ = nullptr;
  bool toolbar_owner_mutex_acquired_ = false;
  HWND toolbar_tooltip_window_ = nullptr;
  HWND context_menu_window_ = nullptr;
  HWND context_submenu_window_ = nullptr;
  ITfContext* active_context_ = nullptr;
  HWND control_window_ = nullptr;
  POINT last_candidate_anchor_{};
  POINT toolbar_position_{};
  bool has_last_candidate_anchor_ = false;
  bool has_toolbar_position_ = false;
  bool toolbar_position_user_ = false;
  bool toolbar_dragging_ = false;
  bool toolbar_vertical_layout_ = false;
  POINT toolbar_drag_start_cursor_{};
  POINT toolbar_drag_start_origin_{};
  std::vector<int> toolbar_visible_items_;
  int hovered_toolbar_item_ = 0;
  int pressed_toolbar_item_ = 0;
  int toolbar_tooltip_item_ = 0;
  POINT toolbar_tooltip_anchor_{};
  bool has_toolbar_tooltip_anchor_ = false;
  bool toolbar_mouse_tracking_ = false;
  bool toolbar_host_process_ = false;
  int hovered_context_menu_row_ = -1;
  bool context_menu_toolbar_mode_ = false;
  int context_submenu_parent_row_ = -1;
  int hovered_context_submenu_row_ = -1;
  bool context_menu_mouse_tracking_ = false;
  bool context_submenu_mouse_tracking_ = false;
  bool toolbar_visible_ = false;
  bool super_abbrev_enabled_ = true;
  bool chinese_punctuation_mode_ = true;
  bool full_shape_mode_ = false;
  bool simplified_charset_ = true;
  bool ascii_mode_ = false;
  bool caps_lock_ascii_mode_ = false;
  StatusTipDetail status_tip_detail_ = StatusTipDetail::kNone;
  StatusTipIconMode status_tip_icon_mode_ = StatusTipIconMode::kChinesePunctuation;
  bool status_tip_enabled_ = true;
  std::wstring status_tip_blacklist_;
  DWORD status_tip_settings_tick_ = 0;
  bool has_input_focus_ = false;
  bool horizontal_candidate_layout_ = true;
  bool expanded_candidate_window_ = false;
  int hovered_candidate_tool_ = 0;
  int pressed_candidate_tool_ = 0;
  POINT candidate_tooltip_anchor_{};
  bool has_candidate_tooltip_anchor_ = false;
  bool candidate_mouse_tracking_ = false;
  int compact_candidate_count_ = 7;
  int candidate_font_size_level_ = 0;
  std::wstring candidate_font_family_ = L"misans";
  std::wstring theme_mode_ = L"dark";
  std::wstring theme_preset_ = L"default_dark";
  bool apps_use_light_theme_ = true;
  bool system_uses_light_theme_ = true;
  bool shift_key_down_ = false;
  bool input_mode_shortcut_down_ = false;
  WPARAM input_mode_shortcut_key_ = 0;
  bool suppress_next_composition_termination_ = false;
  bool pending_candidate_continuation_ = false;
};

}  // namespace fp::tsf
