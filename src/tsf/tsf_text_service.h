#pragma once

#include "core/rime_engine.h"

#include <msctf.h>
#include <windows.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace fp::tsf {

class TsfTextService final : public ITfTextInputProcessor,
                             public ITfKeyEventSink,
                             public ITfCompositionSink {
 public:
  TsfTextService();
  TsfTextService(const TsfTextService&) = delete;
  TsfTextService& operator=(const TsfTextService&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  STDMETHODIMP Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) override;
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

  void HandleLangBarMenuCommand(UINT command_id);
  void ToggleToolbarWindow();
  [[nodiscard]] bool toolbar_visible() const noexcept { return toolbar_visible_; }
  [[nodiscard]] bool full_shape_mode() const noexcept { return full_shape_mode_; }
  [[nodiscard]] bool simplified_charset() const noexcept { return simplified_charset_; }

 private:
  ~TsfTextService();

  bool IsComposing() const noexcept;
  bool IsModifierShortcutActive() const;
  bool IsKeyHandled(WPARAM wparam, LPARAM lparam) const;
  bool HandleKey(ITfContext* context, WPARAM wparam, LPARAM lparam);
  void InitializeRime();
  void UninitializeRime();
  void RefreshCandidates();
  void ChangeCandidatePage(ITfContext* context, int delta);
  bool UpdatePreedit(ITfContext* context, const std::wstring& text);
  bool CommitText(ITfContext* context, const std::wstring& text);
  bool CommitCandidate(ITfContext* context, size_t candidate_index);
  bool CommitCandidateFromMouse(size_t candidate_index);
  bool CancelComposition(ITfContext* context);
  void ClearCompositionState();
  void ShowCandidateWindow(ITfContext* context = nullptr);
  void HideCandidateWindow();
  void DestroyCandidateWindow();
  POINT CandidateWindowAnchor(ITfContext* context);
  void DrawCandidateWindow(HDC dc);
  ATOM EnsureCandidateWindowClass();
  LRESULT CandidateWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticCandidateWindowProc(HWND window,
                                                    UINT message,
                                                    WPARAM wparam,
                                                    LPARAM lparam);
  void ShowToolbarWindow();
  void HideToolbarWindow();
  void DestroyToolbarWindow();
  void PositionToolbarWindow();
  void DrawToolbarWindow(HDC dc);
  ATOM EnsureToolbarWindowClass();
  LRESULT ToolbarWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticToolbarWindowProc(HWND window,
                                                  UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam);
  void OpenConfigApp();
  void OpenRimeUserDirectory();

  std::atomic<unsigned long> ref_count_{1};
  ITfThreadMgr* thread_mgr_ = nullptr;
  ITfKeystrokeMgr* keystroke_mgr_ = nullptr;
  ITfLangBarItemMgr* lang_bar_item_mgr_ = nullptr;
  ITfLangBarItemButton* input_mode_item_ = nullptr;
  TfClientId client_id_ = 0;
  std::unique_ptr<fp::core::RimeEngine> rime_;
  bool rime_ready_ = false;
  std::string composition_input_;
  LONG preedit_length_ = 0;
  int candidate_page_index_ = 0;
  bool has_previous_candidate_page_ = false;
  bool has_next_candidate_page_ = false;
  std::vector<fp::core::RimeCandidateView> candidates_;
  ITfComposition* composition_ = nullptr;
  HWND candidate_window_ = nullptr;
  HWND toolbar_window_ = nullptr;
  ITfContext* active_context_ = nullptr;
  bool toolbar_visible_ = true;
  bool full_shape_mode_ = false;
  bool simplified_charset_ = true;
};

}  // namespace fp::tsf
