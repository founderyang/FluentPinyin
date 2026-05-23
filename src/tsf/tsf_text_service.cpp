#include "tsf/tsf_text_service.h"

#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "tsf/guids.h"
#include "tsf/module.h"

#include <ctffunc.h>
#include <oleauto.h>
#include <shellapi.h>

#include <atomic>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <initializer_list>

namespace fp::tsf {
namespace {

enum LangBarMenuCommand : UINT {
  kMenuFullShape = 100,
  kMenuFullShapeFull,
  kMenuFullShapeHalf,
  kMenuCharset,
  kMenuCharsetSimplified,
  kMenuEmoji,
  kMenuCustomPhrases,
  kMenuProfessionalDictionaries,
  kMenuKeyConfig,
  kMenuToolbar,
  kMenuSettings,
  kMenuOpenUserDirectory,
  kMenuFeedback,
};

HICON CreateTextIcon(const wchar_t* text) {
  constexpr int kFallbackSize = 16;
  const int width =
      GetSystemMetrics(SM_CXSMICON) > 0 ? GetSystemMetrics(SM_CXSMICON) : kFallbackSize;
  const int height =
      GetSystemMetrics(SM_CYSMICON) > 0 ? GetSystemMetrics(SM_CYSMICON) : kFallbackSize;

  HDC screen_dc = GetDC(nullptr);
  if (screen_dc == nullptr) {
    return nullptr;
  }

  HDC memory_dc = CreateCompatibleDC(screen_dc);
  if (memory_dc == nullptr) {
    ReleaseDC(nullptr, screen_dc);
    return nullptr;
  }

  BITMAPINFO bitmap_info{};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = width;
  bitmap_info.bmiHeader.biHeight = -height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP color_bitmap =
      CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (color_bitmap == nullptr || bits == nullptr) {
    if (color_bitmap != nullptr) {
      DeleteObject(color_bitmap);
    }
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);
    return nullptr;
  }

  std::memset(bits, 0, static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

  HGDIOBJ old_bitmap = SelectObject(memory_dc, color_bitmap);
  HFONT font = CreateFontW(-(height - 3),
                           0,
                           0,
                           0,
                           FW_BOLD,
                           FALSE,
                           FALSE,
                           FALSE,
                           DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE,
                           L"Microsoft YaHei UI");
  HGDIOBJ old_font = nullptr;
  if (font != nullptr) {
    old_font = SelectObject(memory_dc, font);
  }

  SetBkMode(memory_dc, TRANSPARENT);
  SetTextColor(memory_dc, RGB(32, 32, 32));
  RECT text_rect{0, -1, width, height - 1};
  DrawTextW(memory_dc,
            text,
            -1,
            &text_rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);

  auto* pixels = static_cast<unsigned char*>(bits);
  const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
  for (size_t index = 0; index < pixel_count; ++index) {
    unsigned char* pixel = pixels + index * 4;
    if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0) {
      pixel[3] = 255;
    }
  }

  const int mask_stride = ((width + 15) / 16) * 2;
  std::vector<unsigned char> mask_bits(static_cast<size_t>(mask_stride) *
                                       static_cast<size_t>(height));
  HBITMAP mask_bitmap = CreateBitmap(width, height, 1, 1, mask_bits.data());
  ICONINFO icon_info{};
  icon_info.fIcon = TRUE;
  icon_info.hbmMask = mask_bitmap;
  icon_info.hbmColor = color_bitmap;
  HICON icon = mask_bitmap != nullptr ? CreateIconIndirect(&icon_info) : nullptr;

  if (old_font != nullptr) {
    SelectObject(memory_dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  if (old_bitmap != nullptr) {
    SelectObject(memory_dc, old_bitmap);
  }
  if (mask_bitmap != nullptr) {
    DeleteObject(mask_bitmap);
  }
  DeleteObject(color_bitmap);
  DeleteDC(memory_dc);
  ReleaseDC(nullptr, screen_dc);

  return icon;
}

class InputModeLangBarItem final : public ITfLangBarItemButton {
 public:
  explicit InputModeLangBarItem(TsfTextService* owner)
      : owner_(owner), icon_(CreateTextIcon(kLanguageListLabel)) {
    if (owner_ != nullptr) {
      owner_->AddRef();
    }
  }
  InputModeLangBarItem(const InputModeLangBarItem&) = delete;
  InputModeLangBarItem& operator=(const InputModeLangBarItem&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) ||
        IsEqualIID(riid, IID_ITfLangBarItemButton)) {
      *object = static_cast<ITfLangBarItemButton*>(this);
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }

    return count;
  }

  STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* info) override {
    if (info == nullptr) {
      return E_POINTER;
    }

    info->clsidService = kTextServiceClsid;
    info->guidItem = GUID_LBI_INPUTMODE;
    info->dwStyle =
        TF_LBI_STYLE_SHOWNINTRAYONLY | TF_LBI_STYLE_BTN_TOGGLE | TF_LBI_STYLE_TEXTCOLORICON;
    info->ulSort = 0;
    wcsncpy_s(info->szDescription, kProfileDescription, _TRUNCATE);
    return S_OK;
  }

  STDMETHODIMP GetStatus(DWORD* status) override {
    if (status == nullptr) {
      return E_POINTER;
    }

    *status = TF_LBI_STATUS_BTN_TOGGLED;
    return S_OK;
  }

  STDMETHODIMP Show(BOOL) override { return S_OK; }

  STDMETHODIMP GetTooltipString(BSTR* tooltip) override {
    if (tooltip == nullptr) {
      return E_POINTER;
    }

    *tooltip = SysAllocString(kProfileDescription);
    return *tooltip != nullptr ? S_OK : E_OUTOFMEMORY;
  }

  STDMETHODIMP OnClick(TfLBIClick, POINT, const RECT*) override { return S_OK; }

  STDMETHODIMP InitMenu(ITfMenu* menu) override {
    if (menu == nullptr) {
      return E_POINTER;
    }

    AddSubMenu(menu,
               kMenuFullShape,
               owner_ != nullptr && owner_->full_shape_mode() ? L"\u5168\u534A\u89D2(\u5168\u89D2)"
                                                              : L"\u5168\u534A\u89D2(\u534A\u89D2)",
               {{kMenuFullShapeFull, L"\u5168\u89D2", owner_ != nullptr && owner_->full_shape_mode()},
                {kMenuFullShapeHalf,
                 L"\u534A\u89D2",
                 owner_ == nullptr || !owner_->full_shape_mode()}});
    AddSubMenu(menu,
               kMenuCharset,
               L"\u5B57\u7B26\u96C6(\u7B80\u4F53)",
               {{kMenuCharsetSimplified,
                 L"\u7B80\u4F53",
                 owner_ == nullptr || owner_->simplified_charset()}});
    AddSeparator(menu);
    AddItem(menu, kMenuEmoji, L"\u8868\u60C5\u7B26\u53F7\u548C\u7B26\u53F7");
    AddSeparator(menu);
    AddItem(menu, kMenuCustomPhrases, L"\u7528\u6237\u81EA\u5B9A\u4E49\u77ED\u8BED");
    AddItem(menu, kMenuProfessionalDictionaries, L"\u4E13\u4E1A\u8BCD\u5178");
    AddItem(menu, kMenuOpenUserDirectory, L"\u6253\u5F00\u7528\u6237\u76EE\u5F55");
    AddSeparator(menu);
    AddItem(menu, kMenuKeyConfig, L"\u6309\u952E\u914D\u7F6E");
    AddSeparator(menu);
    AddItem(menu,
            kMenuToolbar,
            owner_ != nullptr && owner_->toolbar_visible()
                ? L"\u8F93\u5165\u6CD5\u5DE5\u5177\u680F(\u5F00)"
                : L"\u8F93\u5165\u6CD5\u5DE5\u5177\u680F(\u5173)",
            owner_ != nullptr && owner_->toolbar_visible());
    AddItem(menu, kMenuSettings, L"\u8BBE\u7F6E");
    AddItem(menu, kMenuFeedback, L"\u5E2E\u52A9\u4E0E\u53CD\u9988");
    return S_OK;
  }

  STDMETHODIMP OnMenuSelect(UINT id) override {
    if (owner_ != nullptr) {
      owner_->HandleLangBarMenuCommand(id);
    }
    return S_OK;
  }

  STDMETHODIMP GetIcon(HICON* icon) override {
    if (icon == nullptr) {
      return E_POINTER;
    }

    *icon = icon_ != nullptr ? CopyIcon(icon_) : nullptr;
    return *icon != nullptr ? S_OK : E_FAIL;
  }

  STDMETHODIMP GetText(BSTR* text) override {
    if (text == nullptr) {
      return E_POINTER;
    }

    *text = SysAllocString(kLanguageListLabel);
    return *text != nullptr ? S_OK : E_OUTOFMEMORY;
  }

 private:
  ~InputModeLangBarItem() {
    if (owner_ != nullptr) {
      owner_->Release();
      owner_ = nullptr;
    }
    if (icon_ != nullptr) {
      DestroyIcon(icon_);
      icon_ = nullptr;
    }
  }

  struct SubMenuItem {
    UINT id;
    const wchar_t* text;
    bool checked;
  };

  void AddItem(ITfMenu* menu,
               UINT id,
               const wchar_t* text,
               bool checked = false,
               DWORD extra_flags = 0) {
    const DWORD flags = extra_flags | (checked ? TF_LBMENUF_CHECKED : 0);
    menu->AddMenuItem(id, flags, nullptr, nullptr, text, static_cast<ULONG>(wcslen(text)), nullptr);
  }

  void AddSeparator(ITfMenu* menu) {
    menu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr, nullptr, 0, nullptr);
  }

  void AddSubMenu(ITfMenu* menu,
                  UINT id,
                  const wchar_t* text,
                  std::initializer_list<SubMenuItem> items) {
    ITfMenu* submenu = nullptr;
    menu->AddMenuItem(id,
                      TF_LBMENUF_SUBMENU,
                      nullptr,
                      nullptr,
                      text,
                      static_cast<ULONG>(wcslen(text)),
                      &submenu);
    if (submenu == nullptr) {
      return;
    }
    for (const auto& item : items) {
      AddItem(submenu, item.id, item.text, item.checked, TF_LBMENUF_RADIOCHECKED);
    }
    submenu->Release();
  }

  std::atomic<unsigned long> ref_count_{1};
  TsfTextService* owner_ = nullptr;
  HICON icon_ = nullptr;
};

bool ReplaceTextBeforeSelection(ITfContext* context,
                                TfEditCookie edit_cookie,
                                LONG old_length,
                                std::wstring_view replacement) {
  if (context == nullptr || old_length < 0) {
    return false;
  }

  TF_SELECTION selection{};
  ULONG fetched = 0;
  HRESULT result =
      context->GetSelection(edit_cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
  if (FAILED(result) || fetched == 0 || selection.range == nullptr) {
    return false;
  }

  ITfRange* range = selection.range;
  bool succeeded = true;

  if (old_length > 0) {
    LONG shifted = 0;
    result = range->ShiftStart(edit_cookie, -old_length, &shifted, nullptr);
    if (FAILED(result)) {
      succeeded = false;
    }
  }

  if (succeeded) {
    result = range->SetText(edit_cookie,
                            0,
                            replacement.data(),
                            static_cast<LONG>(replacement.size()));
    succeeded = SUCCEEDED(result);
  }

  if (succeeded) {
    range->Collapse(edit_cookie, TF_ANCHOR_END);
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    result = context->SetSelection(edit_cookie, 1, &selection);
    succeeded = SUCCEEDED(result);
  }

  range->Release();
  return succeeded;
}

class ReplaceTextEditSession final : public ITfEditSession {
 public:
  ReplaceTextEditSession(ITfContext* context,
                         LONG old_length,
                         std::wstring replacement)
      : context_(context),
        old_length_(old_length),
        replacement_(std::move(replacement)) {
    if (context_ != nullptr) {
      context_->AddRef();
    }
  }

  ReplaceTextEditSession(const ReplaceTextEditSession&) = delete;
  ReplaceTextEditSession& operator=(const ReplaceTextEditSession&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
      *object = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    succeeded_ =
        ReplaceTextBeforeSelection(context_, edit_cookie, old_length_, replacement_);
    return succeeded_ ? S_OK : E_FAIL;
  }

  bool succeeded() const noexcept { return succeeded_; }

 private:
  ~ReplaceTextEditSession() {
    if (context_ != nullptr) {
      context_->Release();
      context_ = nullptr;
    }
  }

  std::atomic<unsigned long> ref_count_{1};
  ITfContext* context_ = nullptr;
  LONG old_length_ = 0;
  std::wstring replacement_;
  bool succeeded_ = false;
};

bool RequestTextReplacement(TfClientId client_id,
                            ITfContext* context,
                            LONG old_length,
                            const std::wstring& replacement) {
  auto* session = new (std::nothrow) ReplaceTextEditSession(context, old_length, replacement);
  if (session == nullptr) {
    return false;
  }

  HRESULT edit_result = E_FAIL;
  const HRESULT request_result = context->RequestEditSession(
      client_id, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
  const bool succeeded =
      SUCCEEDED(request_result) && SUCCEEDED(edit_result) && session->succeeded();
  session->Release();
  return succeeded;
}

bool SetRangeText(ITfRange* range, TfEditCookie edit_cookie, std::wstring_view text) {
  if (range == nullptr) {
    return false;
  }

  const HRESULT result =
      range->SetText(edit_cookie, 0, text.data(), static_cast<LONG>(text.size()));
  return SUCCEEDED(result);
}

bool MoveSelectionToRangeEnd(ITfContext* context, TfEditCookie edit_cookie, ITfRange* range) {
  if (context == nullptr || range == nullptr) {
    return false;
  }

  ITfRange* caret_range = nullptr;
  HRESULT result = range->Clone(&caret_range);
  if (FAILED(result) || caret_range == nullptr) {
    return false;
  }

  result = caret_range->Collapse(edit_cookie, TF_ANCHOR_END);
  if (FAILED(result)) {
    caret_range->Release();
    return false;
  }

  TF_SELECTION selection{};
  selection.range = caret_range;
  selection.style.ase = TF_AE_NONE;
  selection.style.fInterimChar = FALSE;
  result = context->SetSelection(edit_cookie, 1, &selection);
  caret_range->Release();
  return SUCCEEDED(result);
}

enum class CompositionEditAction {
  kUpdate,
  kCommit,
  kCancel,
};

class CompositionEditSession final : public ITfEditSession {
 public:
  CompositionEditSession(ITfContext* context,
                         ITfCompositionSink* sink,
                         ITfComposition** composition_slot,
                         std::wstring text,
                         CompositionEditAction action)
      : context_(context),
        sink_(sink),
        composition_slot_(composition_slot),
        text_(std::move(text)),
        action_(action) {
    if (context_ != nullptr) {
      context_->AddRef();
    }
    if (sink_ != nullptr) {
      sink_->AddRef();
    }
  }

  CompositionEditSession(const CompositionEditSession&) = delete;
  CompositionEditSession& operator=(const CompositionEditSession&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
      *object = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    switch (action_) {
      case CompositionEditAction::kUpdate:
        succeeded_ = Update(edit_cookie);
        break;
      case CompositionEditAction::kCommit:
        succeeded_ = Commit(edit_cookie);
        break;
      case CompositionEditAction::kCancel:
        succeeded_ = Cancel(edit_cookie);
        break;
    }
    return succeeded_ ? S_OK : E_FAIL;
  }

  bool succeeded() const noexcept { return succeeded_; }

 private:
  ~CompositionEditSession() {
    if (sink_ != nullptr) {
      sink_->Release();
      sink_ = nullptr;
    }
    if (context_ != nullptr) {
      context_->Release();
      context_ = nullptr;
    }
  }

  bool Start(TfEditCookie edit_cookie) {
    if (context_ == nullptr || sink_ == nullptr || composition_slot_ == nullptr ||
        *composition_slot_ != nullptr) {
      return false;
    }

    TF_SELECTION selection{};
    ULONG fetched = 0;
    HRESULT result =
        context_->GetSelection(edit_cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
    if (FAILED(result) || fetched == 0 || selection.range == nullptr) {
      return false;
    }

    bool succeeded = SetRangeText(selection.range, edit_cookie, text_);
    if (succeeded) {
      succeeded = MoveSelectionToRangeEnd(context_, edit_cookie, selection.range);
    }
    if (succeeded) {
      ITfContextComposition* context_composition = nullptr;
      result = context_->QueryInterface(IID_ITfContextComposition,
                                        reinterpret_cast<void**>(&context_composition));
      if (SUCCEEDED(result) && context_composition != nullptr) {
        ITfComposition* composition = nullptr;
        result = context_composition->StartComposition(
            edit_cookie, selection.range, sink_, &composition);
        if (SUCCEEDED(result) && composition != nullptr) {
          *composition_slot_ = composition;
        } else {
          succeeded = false;
        }
        context_composition->Release();
      } else {
        succeeded = false;
      }
    }

    selection.range->Release();
    return succeeded;
  }

  bool Update(TfEditCookie edit_cookie) {
    if (composition_slot_ == nullptr) {
      return false;
    }

    if (*composition_slot_ == nullptr) {
      return Start(edit_cookie);
    }

    ITfRange* range = nullptr;
    HRESULT result = (*composition_slot_)->GetRange(&range);
    if (FAILED(result) || range == nullptr) {
      return false;
    }

    bool succeeded = SetRangeText(range, edit_cookie, text_);
    if (succeeded) {
      succeeded = MoveSelectionToRangeEnd(context_, edit_cookie, range);
    }
    range->Release();
    return succeeded;
  }

  bool Commit(TfEditCookie edit_cookie) {
    if (composition_slot_ == nullptr || *composition_slot_ == nullptr) {
      return false;
    }

    ITfComposition* composition = *composition_slot_;
    ITfRange* range = nullptr;
    HRESULT result = composition->GetRange(&range);
    if (FAILED(result) || range == nullptr) {
      return false;
    }

    bool text_succeeded = SetRangeText(range, edit_cookie, text_);
    if (text_succeeded) {
      text_succeeded = MoveSelectionToRangeEnd(context_, edit_cookie, range);
    }
    range->Release();
    if (!text_succeeded) {
      return false;
    }

    *composition_slot_ = nullptr;
    composition->EndComposition(edit_cookie);
    composition->Release();
    return true;
  }

  bool Cancel(TfEditCookie edit_cookie) {
    text_.clear();
    return Commit(edit_cookie);
  }

  std::atomic<unsigned long> ref_count_{1};
  ITfContext* context_ = nullptr;
  ITfCompositionSink* sink_ = nullptr;
  ITfComposition** composition_slot_ = nullptr;
  std::wstring text_;
  CompositionEditAction action_;
  bool succeeded_ = false;
};

bool RequestCompositionEdit(TfClientId client_id,
                            ITfContext* context,
                            ITfCompositionSink* sink,
                            ITfComposition** composition_slot,
                            const std::wstring& text,
                            CompositionEditAction action) {
  if (context == nullptr || sink == nullptr || composition_slot == nullptr) {
    return false;
  }

  auto* session =
      new (std::nothrow) CompositionEditSession(context, sink, composition_slot, text, action);
  if (session == nullptr) {
    return false;
  }

  HRESULT edit_result = E_FAIL;
  const HRESULT request_result = context->RequestEditSession(
      client_id, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
  const bool succeeded =
      SUCCEEDED(request_result) && SUCCEEDED(edit_result) && session->succeeded();
  session->Release();
  return succeeded;
}

class AnchorEditSession final : public ITfEditSession {
 public:
  AnchorEditSession(ITfContext* context, POINT* anchor) : context_(context), anchor_(anchor) {
    if (context_ != nullptr) {
      context_->AddRef();
    }
  }

  AnchorEditSession(const AnchorEditSession&) = delete;
  AnchorEditSession& operator=(const AnchorEditSession&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
      *object = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    if (context_ == nullptr || anchor_ == nullptr) {
      return E_FAIL;
    }

    TF_SELECTION selection{};
    ULONG fetched = 0;
    HRESULT result =
        context_->GetSelection(edit_cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
    if (FAILED(result) || fetched == 0 || selection.range == nullptr) {
      return E_FAIL;
    }

    ITfContextView* view = nullptr;
    result = context_->GetActiveView(&view);
    if (SUCCEEDED(result) && view != nullptr) {
      RECT text_rect{};
      BOOL clipped = FALSE;
      result = view->GetTextExt(edit_cookie, selection.range, &text_rect, &clipped);
      if (SUCCEEDED(result)) {
        anchor_->x = text_rect.left;
        anchor_->y = text_rect.bottom;
        succeeded_ = true;
      }
      view->Release();
    }

    selection.range->Release();
    return succeeded_ ? S_OK : E_FAIL;
  }

  bool succeeded() const noexcept { return succeeded_; }

 private:
  ~AnchorEditSession() {
    if (context_ != nullptr) {
      context_->Release();
      context_ = nullptr;
    }
  }

  std::atomic<unsigned long> ref_count_{1};
  ITfContext* context_ = nullptr;
  POINT* anchor_ = nullptr;
  bool succeeded_ = false;
};

bool RequestSelectionAnchor(TfClientId client_id, ITfContext* context, POINT* anchor) {
  if (context == nullptr || anchor == nullptr) {
    return false;
  }

  auto* session = new (std::nothrow) AnchorEditSession(context, anchor);
  if (session == nullptr) {
    return false;
  }

  HRESULT edit_result = E_FAIL;
  const HRESULT request_result =
      context->RequestEditSession(client_id, session, TF_ES_SYNC | TF_ES_READ, &edit_result);
  const bool succeeded =
      SUCCEEDED(request_result) && SUCCEEDED(edit_result) && session->succeeded();
  session->Release();
  return succeeded;
}

std::filesystem::path ModuleDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(g_module_instance, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return std::filesystem::current_path();
  }
  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
}

std::wstring AsciiToWide(std::string_view value) {
  std::wstring result;
  result.reserve(value.size());
  for (const unsigned char ch : value) {
    result.push_back(static_cast<wchar_t>(ch));
  }
  return result;
}

bool IsVirtualKeyDown(int virtual_key) {
  return (GetKeyState(virtual_key) & 0x8000) != 0;
}

bool IsPrintableAsciiPunctuation(WPARAM wparam) {
  switch (wparam) {
    case VK_OEM_COMMA:
    case VK_OEM_PERIOD:
    case VK_OEM_1:
    case VK_OEM_2:
    case VK_OEM_3:
    case VK_OEM_4:
    case VK_OEM_5:
    case VK_OEM_6:
    case VK_OEM_7:
    case VK_OEM_MINUS:
    case VK_OEM_PLUS:
      return true;
    default:
      return false;
  }
}

std::wstring PunctuationForKey(WPARAM wparam) {
  const bool shifted = IsVirtualKeyDown(VK_SHIFT);
  switch (wparam) {
    case VK_OEM_COMMA:
      return shifted ? L"<" : L"\uFF0C";
    case VK_OEM_PERIOD:
      return shifted ? L">" : L"\u3002";
    case VK_OEM_1:
      return shifted ? L"\uFF1A" : L"\uFF1B";
    case VK_OEM_2:
      return shifted ? L"\uFF1F" : L"\u3001";
    case VK_OEM_3:
      return shifted ? L"~" : L"\u00B7";
    case VK_OEM_4:
      return shifted ? L"\u3010" : L"\uFF3B";
    case VK_OEM_5:
      return shifted ? L"|" : L"\u3001";
    case VK_OEM_6:
      return shifted ? L"\u3011" : L"\uFF3D";
    case VK_OEM_7:
      return shifted ? L"\u201C" : L"\u2018";
    case VK_OEM_MINUS:
      return shifted ? L"\u2014" : L"-";
    case VK_OEM_PLUS:
      return shifted ? L"+" : L"=";
    default:
      return {};
  }
}

}  // namespace

TsfTextService::TsfTextService() {
  ++g_object_count;
  fp::LogInfo(L"tsf", L"TsfTextService created.");
}

TsfTextService::~TsfTextService() {
  DestroyCandidateWindow();
  DestroyToolbarWindow();
  UninitializeRime();

  if (composition_ != nullptr) {
    composition_->Release();
    composition_ = nullptr;
  }

  if (active_context_ != nullptr) {
    active_context_->Release();
    active_context_ = nullptr;
  }

  if (keystroke_mgr_ != nullptr && client_id_ != 0) {
    keystroke_mgr_->UnadviseKeyEventSink(client_id_);
  }
  if (keystroke_mgr_ != nullptr) {
    keystroke_mgr_->Release();
    keystroke_mgr_ = nullptr;
  }

  if (lang_bar_item_mgr_ != nullptr && input_mode_item_ != nullptr) {
    lang_bar_item_mgr_->RemoveItem(input_mode_item_);
  }
  if (input_mode_item_ != nullptr) {
    input_mode_item_->Release();
    input_mode_item_ = nullptr;
  }
  if (lang_bar_item_mgr_ != nullptr) {
    lang_bar_item_mgr_->Release();
    lang_bar_item_mgr_ = nullptr;
  }

  if (thread_mgr_ != nullptr) {
    thread_mgr_->Release();
    thread_mgr_ = nullptr;
  }

  --g_object_count;
  fp::LogInfo(L"tsf", L"TsfTextService destroyed.");
}

STDMETHODIMP TsfTextService::QueryInterface(REFIID riid, void** object) {
  if (object == nullptr) {
    return E_POINTER;
  }

  *object = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
    *object = static_cast<ITfTextInputProcessor*>(this);
    AddRef();
    return S_OK;
  }

  if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
    *object = static_cast<ITfKeyEventSink*>(this);
    AddRef();
    return S_OK;
  }

  if (IsEqualIID(riid, IID_ITfCompositionSink)) {
    *object = static_cast<ITfCompositionSink*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) TsfTextService::AddRef() {
  return ++ref_count_;
}

STDMETHODIMP_(ULONG) TsfTextService::Release() {
  const ULONG count = --ref_count_;
  if (count == 0) {
    delete this;
  }

  return count;
}

STDMETHODIMP TsfTextService::Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) {
  if (thread_mgr == nullptr) {
    return E_INVALIDARG;
  }

  if (thread_mgr_ != nullptr) {
    thread_mgr_->Release();
  }

  thread_mgr_ = thread_mgr;
  thread_mgr_->AddRef();
  client_id_ = client_id;

  HRESULT result = thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr,
                                               reinterpret_cast<void**>(&keystroke_mgr_));
  if (SUCCEEDED(result) && keystroke_mgr_ != nullptr) {
    result = keystroke_mgr_->AdviseKeyEventSink(client_id_, this, TRUE);
    if (FAILED(result)) {
      fp::LogWarning(L"tsf", L"AdviseKeyEventSink failed.");
      keystroke_mgr_->Release();
      keystroke_mgr_ = nullptr;
    }
  } else {
    fp::LogWarning(L"tsf", L"ITfKeystrokeMgr unavailable.");
  }

  InitializeRime();

  result = CoCreateInstance(CLSID_TF_LangBarItemMgr,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfLangBarItemMgr,
                            reinterpret_cast<void**>(&lang_bar_item_mgr_));
  if (SUCCEEDED(result)) {
      input_mode_item_ = new (std::nothrow) InputModeLangBarItem(this);
    if (input_mode_item_ == nullptr) {
      fp::LogWarning(L"tsf", L"Failed to allocate input mode language bar item.");
    } else {
      result = lang_bar_item_mgr_->AddItem(input_mode_item_);
      if (FAILED(result)) {
        fp::LogWarning(L"tsf", L"Failed to add input mode language bar item.");
        input_mode_item_->Release();
        input_mode_item_ = nullptr;
      }
    }
  } else {
    fp::LogWarning(L"tsf", L"Failed to create language bar item manager.");
  }

  fp::LogInfo(L"tsf", std::wstring(fp::kProductName) + L" text service activated.");
  return S_OK;
}

STDMETHODIMP TsfTextService::Deactivate() {
  DestroyCandidateWindow();
  DestroyToolbarWindow();
  CancelComposition(nullptr);

  if (composition_ != nullptr) {
    composition_->Release();
    composition_ = nullptr;
  }

  if (active_context_ != nullptr) {
    active_context_->Release();
    active_context_ = nullptr;
  }

  UninitializeRime();

  if (keystroke_mgr_ != nullptr && client_id_ != 0) {
    keystroke_mgr_->UnadviseKeyEventSink(client_id_);
  }
  if (keystroke_mgr_ != nullptr) {
    keystroke_mgr_->Release();
    keystroke_mgr_ = nullptr;
  }

  if (lang_bar_item_mgr_ != nullptr && input_mode_item_ != nullptr) {
    lang_bar_item_mgr_->RemoveItem(input_mode_item_);
  }
  if (input_mode_item_ != nullptr) {
    input_mode_item_->Release();
    input_mode_item_ = nullptr;
  }
  if (lang_bar_item_mgr_ != nullptr) {
    lang_bar_item_mgr_->Release();
    lang_bar_item_mgr_ = nullptr;
  }

  if (thread_mgr_ != nullptr) {
    thread_mgr_->Release();
    thread_mgr_ = nullptr;
  }

  client_id_ = 0;
  fp::LogInfo(L"tsf", std::wstring(fp::kProductName) + L" text service deactivated.");
  return S_OK;
}

STDMETHODIMP TsfTextService::OnSetFocus(BOOL foreground) {
  fp::LogInfo(L"tsf", foreground ? L"Key sink focused." : L"Key sink unfocused.");
  if (foreground && toolbar_visible_) {
    ShowToolbarWindow();
  } else if (!foreground) {
    HideToolbarWindow();
  }
  return S_OK;
}

STDMETHODIMP TsfTextService::OnTestKeyDown(ITfContext* context,
                                           WPARAM wparam,
                                           LPARAM lparam,
                                           BOOL* eaten) {
  (void)context;
  (void)lparam;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  *eaten = IsKeyHandled(wparam, lparam) ? TRUE : FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnKeyDown(ITfContext* context,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       BOOL* eaten) {
  (void)lparam;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  *eaten = HandleKey(context, wparam, lparam) ? TRUE : FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnTestKeyUp(ITfContext* context,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         BOOL* eaten) {
  (void)context;
  (void)wparam;
  (void)lparam;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  *eaten = FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnKeyUp(ITfContext* context,
                                     WPARAM wparam,
                                     LPARAM lparam,
                                     BOOL* eaten) {
  (void)context;
  (void)wparam;
  (void)lparam;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  *eaten = FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnPreservedKey(ITfContext* context,
                                            REFGUID guid,
                                            BOOL* eaten) {
  (void)context;
  (void)guid;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  *eaten = FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnCompositionTerminated(TfEditCookie edit_cookie,
                                                     ITfComposition* composition) {
  (void)edit_cookie;
  if (composition_ == composition && composition_ != nullptr) {
    composition_->Release();
    composition_ = nullptr;
  }
  ClearCompositionState();
  HideCandidateWindow();
  return S_OK;
}

bool TsfTextService::IsComposing() const noexcept {
  return !composition_input_.empty();
}

bool TsfTextService::IsModifierShortcutActive() const {
  return IsVirtualKeyDown(VK_CONTROL) || IsVirtualKeyDown(VK_MENU) ||
         IsVirtualKeyDown(VK_LWIN) || IsVirtualKeyDown(VK_RWIN);
}

bool TsfTextService::IsKeyHandled(WPARAM wparam, LPARAM lparam) const {
  (void)lparam;
  if (IsModifierShortcutActive()) {
    return false;
  }

  if (wparam >= 'A' && wparam <= 'Z') {
    return true;
  }

  if (IsComposing()) {
    if (wparam == VK_BACK || wparam == VK_ESCAPE || wparam == VK_RETURN ||
        wparam == VK_SPACE || wparam == VK_PRIOR || wparam == VK_NEXT ||
        wparam == VK_LEFT || wparam == VK_RIGHT) {
      return true;
    }
    if (wparam >= '1' && wparam <= '8') {
      return true;
    }
    if (IsPrintableAsciiPunctuation(wparam)) {
      return true;
    }
  }

  return false;
}

bool TsfTextService::HandleKey(ITfContext* context, WPARAM wparam, LPARAM lparam) {
  (void)lparam;
  if (context == nullptr) {
    return false;
  }
  if (IsModifierShortcutActive()) {
    return false;
  }

  if (active_context_ != context) {
    if (active_context_ != nullptr) {
      active_context_->Release();
    }
    active_context_ = context;
    active_context_->AddRef();
  }

  if (wparam >= 'A' && wparam <= 'Z') {
    composition_input_.push_back(static_cast<char>(std::tolower(static_cast<int>(wparam))));
    candidate_page_index_ = 0;
    RefreshCandidates();
    const bool succeeded = UpdatePreedit(context, AsciiToWide(composition_input_));
    ShowCandidateWindow(context);
    return succeeded;
  }

  if (!IsComposing()) {
    return false;
  }

  if (wparam == VK_BACK) {
    composition_input_.pop_back();
    candidate_page_index_ = 0;
    RefreshCandidates();
    if (composition_input_.empty()) {
      return CancelComposition(context);
    }
    const bool succeeded = UpdatePreedit(context, AsciiToWide(composition_input_));
    ShowCandidateWindow(context);
    return succeeded;
  }

  if (wparam == VK_ESCAPE) {
    return CancelComposition(context);
  }

  if (wparam == VK_RETURN) {
    return CommitText(context, AsciiToWide(composition_input_));
  }

  if (wparam == VK_SPACE) {
    if (!candidates_.empty()) {
      return CommitCandidate(context, 0);
    }
    return CommitText(context, AsciiToWide(composition_input_));
  }

  if (wparam == VK_NEXT || wparam == VK_RIGHT) {
    ChangeCandidatePage(context, 1);
    return true;
  }

  if (wparam == VK_PRIOR || wparam == VK_LEFT) {
    ChangeCandidatePage(context, -1);
    return true;
  }

  if (wparam >= '1' && wparam <= '8') {
    const size_t index = static_cast<size_t>(wparam - '1');
    if (index < candidates_.size()) {
      return CommitCandidate(context, index);
    }
    return true;
  }

  if (IsPrintableAsciiPunctuation(wparam)) {
    std::wstring commit;
    if (!candidates_.empty()) {
      commit = candidates_[0].text;
    } else {
      commit = AsciiToWide(composition_input_);
    }
    commit += PunctuationForKey(wparam);
    return CommitText(context, commit);
  }

  return false;
}

void TsfTextService::InitializeRime() {
  if (rime_ready_) {
    return;
  }

  rime_ = std::make_unique<fp::core::RimeEngine>();
  const auto status = rime_->Initialize();
  rime_ready_ = status.initialized;
  if (!rime_ready_) {
    fp::LogError(L"tsf", L"Rime init failed: " + status.message);
  }
}

void TsfTextService::UninitializeRime() {
  if (rime_ != nullptr) {
    rime_->Shutdown();
    rime_.reset();
  }
  rime_ready_ = false;
}

void TsfTextService::RefreshCandidates() {
  candidates_.clear();
  has_previous_candidate_page_ = false;
  has_next_candidate_page_ = false;
  if (rime_ready_ && rime_ != nullptr && !composition_input_.empty()) {
    const auto page =
        rime_->GetCandidatePageForInput(composition_input_, candidate_page_index_, 8);
    candidates_ = page.candidates;
    has_previous_candidate_page_ = page.has_previous_page;
    has_next_candidate_page_ = page.has_next_page;
  }
  if (candidate_window_ != nullptr) {
    InvalidateRect(candidate_window_, nullptr, TRUE);
  }
}

void TsfTextService::ChangeCandidatePage(ITfContext* context, int delta) {
  if (!IsComposing() || delta == 0) {
    return;
  }

  if (delta > 0 && !has_next_candidate_page_) {
    return;
  }
  if (delta < 0 && candidate_page_index_ == 0) {
    return;
  }

  candidate_page_index_ += delta;
  if (candidate_page_index_ < 0) {
    candidate_page_index_ = 0;
  }
  RefreshCandidates();
  ShowCandidateWindow(context);
}

bool TsfTextService::UpdatePreedit(ITfContext* context, const std::wstring& text) {
  const LONG old_length = preedit_length_;
  bool succeeded = RequestCompositionEdit(client_id_,
                                          context,
                                          static_cast<ITfCompositionSink*>(this),
                                          &composition_,
                                          text,
                                          CompositionEditAction::kUpdate);
  if (!succeeded) {
    succeeded = RequestTextReplacement(client_id_, context, old_length, text);
  }
  if (!succeeded) {
    return false;
  }
  preedit_length_ = static_cast<LONG>(text.size());
  return true;
}

bool TsfTextService::CommitText(ITfContext* context, const std::wstring& text) {
  const LONG old_length = preedit_length_;
  bool succeeded = false;
  if (composition_ != nullptr) {
    succeeded = RequestCompositionEdit(client_id_,
                                       context,
                                       static_cast<ITfCompositionSink*>(this),
                                       &composition_,
                                       text,
                                       CompositionEditAction::kCommit);
  }
  if (!succeeded) {
    succeeded = RequestTextReplacement(client_id_, context, old_length, text);
  }
  if (succeeded) {
    ClearCompositionState();
    HideCandidateWindow();
  }
  return succeeded;
}

bool TsfTextService::CommitCandidate(ITfContext* context, size_t candidate_index) {
  if (candidate_index >= candidates_.size()) {
    return false;
  }
  return CommitText(context, candidates_[candidate_index].text);
}

bool TsfTextService::CommitCandidateFromMouse(size_t candidate_index) {
  if (active_context_ == nullptr || candidate_index >= candidates_.size()) {
    return false;
  }
  return CommitCandidate(active_context_, candidate_index);
}

bool TsfTextService::CancelComposition(ITfContext* context) {
  bool succeeded = true;
  if (context != nullptr && (preedit_length_ > 0 || composition_ != nullptr)) {
    if (composition_ != nullptr) {
      succeeded = RequestCompositionEdit(client_id_,
                                         context,
                                         static_cast<ITfCompositionSink*>(this),
                                         &composition_,
                                         L"",
                                         CompositionEditAction::kCancel);
    }
    if (!succeeded && preedit_length_ > 0) {
      succeeded = RequestTextReplacement(client_id_, context, preedit_length_, L"");
    }
  }
  ClearCompositionState();
  HideCandidateWindow();
  return succeeded;
}

void TsfTextService::ClearCompositionState() {
  composition_input_.clear();
  preedit_length_ = 0;
  candidate_page_index_ = 0;
  has_previous_candidate_page_ = false;
  has_next_candidate_page_ = false;
  candidates_.clear();
}

void TsfTextService::HandleLangBarMenuCommand(UINT command_id) {
  switch (command_id) {
    case kMenuFullShapeFull:
      full_shape_mode_ = true;
      break;
    case kMenuFullShapeHalf:
      full_shape_mode_ = false;
      break;
    case kMenuCharsetSimplified:
      simplified_charset_ = true;
      break;
    case kMenuToolbar:
      ToggleToolbarWindow();
      break;
    case kMenuSettings:
    case kMenuCustomPhrases:
    case kMenuProfessionalDictionaries:
    case kMenuKeyConfig:
      OpenConfigApp();
      break;
    case kMenuOpenUserDirectory:
      OpenRimeUserDirectory();
      break;
    case kMenuEmoji:
      ShellExecuteW(nullptr,
                    L"open",
                    L"ms-shellactivity:emoji",
                    nullptr,
                    nullptr,
                    SW_SHOWNORMAL);
      break;
    case kMenuFeedback:
      OpenConfigApp();
      break;
    default:
      break;
  }

  if (toolbar_window_ != nullptr) {
    InvalidateRect(toolbar_window_, nullptr, TRUE);
  }
}

void TsfTextService::OpenConfigApp() {
  const auto config = ModuleDirectory() / L"fp-config.exe";
  ShellExecuteW(nullptr, L"open", config.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void TsfTextService::OpenRimeUserDirectory() {
  const auto user_dir = fp::GetFpRoamingDataPath() / L"Rime";
  fp::EnsureDirectory(user_dir);
  ShellExecuteW(nullptr, L"open", user_dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void TsfTextService::ToggleToolbarWindow() {
  toolbar_visible_ = !toolbar_visible_;
  if (toolbar_visible_) {
    ShowToolbarWindow();
  } else {
    HideToolbarWindow();
  }
}

void TsfTextService::ShowToolbarWindow() {
  toolbar_visible_ = true;
  if (EnsureToolbarWindowClass() == 0) {
    return;
  }

  if (toolbar_window_ == nullptr) {
    toolbar_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                      L"FluentPinyinToolbarWindow",
                                      L"",
                                      WS_POPUP,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      426,
                                      80,
                                      nullptr,
                                      nullptr,
                                      g_module_instance,
                                      this);
    if (toolbar_window_ == nullptr) {
      return;
    }
  }

  PositionToolbarWindow();
  ShowWindow(toolbar_window_, SW_SHOWNOACTIVATE);
  InvalidateRect(toolbar_window_, nullptr, TRUE);
}

void TsfTextService::HideToolbarWindow() {
  if (toolbar_window_ != nullptr) {
    ShowWindow(toolbar_window_, SW_HIDE);
  }
}

void TsfTextService::DestroyToolbarWindow() {
  if (toolbar_window_ != nullptr) {
    DestroyWindow(toolbar_window_);
    toolbar_window_ = nullptr;
  }
}

void TsfTextService::PositionToolbarWindow() {
  if (toolbar_window_ == nullptr) {
    return;
  }

  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  constexpr int width = 426;
  constexpr int height = 80;
  int x = work_area.right - width - 24;
  int y = work_area.bottom - height - 72;
  if (x < work_area.left) {
    x = work_area.left;
  }
  if (y < work_area.top) {
    y = work_area.top;
  }

  SetWindowPos(toolbar_window_,
               HWND_TOPMOST,
               x,
               y,
               width,
               height,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void TsfTextService::DrawToolbarWindow(HDC dc) {
  RECT client{};
  GetClientRect(toolbar_window_, &client);

  const bool dark_mode = true;
  const COLORREF background_color = dark_mode ? RGB(36, 36, 36) : RGB(252, 252, 252);
  const COLORREF border_color = dark_mode ? RGB(76, 76, 76) : RGB(210, 210, 210);
  const COLORREF text_color = dark_mode ? RGB(245, 245, 245) : RGB(32, 32, 32);
  const COLORREF muted_color = dark_mode ? RGB(170, 170, 170) : RGB(120, 120, 120);

  HBRUSH background = CreateSolidBrush(background_color);
  FillRect(dc, &client, background);
  DeleteObject(background);

  HPEN border = CreatePen(PS_SOLID, 1, border_color);
  HGDIOBJ old_pen = SelectObject(dc, border);
  HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
  RoundRect(dc, client.left, client.top, client.right, client.bottom, 18, 18);
  SelectObject(dc, old_brush);
  SelectObject(dc, old_pen);
  DeleteObject(border);

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, text_color);

  HPEN separator = CreatePen(PS_SOLID, 1, border_color);
  old_pen = SelectObject(dc, separator);
  MoveToEx(dc, 54, 0, nullptr);
  LineTo(dc, 54, client.bottom);
  SelectObject(dc, old_pen);
  DeleteObject(separator);

  HBRUSH grip_brush = CreateSolidBrush(muted_color);
  HGDIOBJ previous_brush = SelectObject(dc, grip_brush);
  HGDIOBJ previous_pen = SelectObject(dc, GetStockObject(NULL_PEN));
  RoundRect(dc, 24, 21, 30, 59, 6, 6);
  SelectObject(dc, previous_pen);
  SelectObject(dc, previous_brush);
  DeleteObject(grip_brush);

  HFONT text_font = CreateFontW(-34,
                                0,
                                0,
                                0,
                                FW_NORMAL,
                                FALSE,
                                FALSE,
                                FALSE,
                                DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE,
                                L"Microsoft YaHei UI");
  HFONT icon_font = CreateFontW(-38,
                                0,
                                0,
                                0,
                                FW_NORMAL,
                                FALSE,
                                FALSE,
                                FALSE,
                                DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE,
                                L"Segoe UI Symbol");
  HGDIOBJ old_font = SelectObject(dc, text_font);

  struct ToolbarItem {
    const wchar_t* text;
    int left;
    bool icon;
  };
  const ToolbarItem items[] = {
      {L"\u4E2D", 70, false},
      {L"\u263E", 132, true},
      {L"\u3002\uFF0C", 194, false},
      {L"\u7B80", 262, false},
      {L"\u263A", 322, true},
      {L"\u2699", 374, true},
  };

  for (const auto& item : items) {
    SelectObject(dc, item.icon ? icon_font : text_font);
    RECT rect{item.left, 14, item.left + 54, 64};
    DrawTextW(dc, item.text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (text_font != nullptr) {
    DeleteObject(text_font);
  }
  if (icon_font != nullptr) {
    DeleteObject(icon_font);
  }
}

ATOM TsfTextService::EnsureToolbarWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = StaticToolbarWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = L"FluentPinyinToolbarWindow";
  atom = RegisterClassExW(&window_class);
  return atom;
}

LRESULT TsfTextService::ToolbarWindowProc(HWND window,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_LBUTTONDOWN: {
      const int x = static_cast<short>(LOWORD(lparam));
      if (x >= 374) {
        OpenConfigApp();
      } else if (x >= 322) {
        HandleLangBarMenuCommand(kMenuEmoji);
      } else if (x >= 262) {
        simplified_charset_ = true;
      } else if (x >= 194) {
        full_shape_mode_ = !full_shape_mode_;
      }
      InvalidateRect(window, nullptr, TRUE);
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(window, &paint);
      DrawToolbarWindow(dc);
      EndPaint(window, &paint);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_NCDESTROY:
      if (toolbar_window_ == window) {
        toolbar_window_ = nullptr;
      }
      break;
    default:
      break;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticToolbarWindowProc(HWND window,
                                                         UINT message,
                                                         WPARAM wparam,
                                                         LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->ToolbarWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

POINT TsfTextService::CandidateWindowAnchor(ITfContext* context) {
  if (context != nullptr) {
    POINT anchor{};
    if (RequestSelectionAnchor(client_id_, context, &anchor)) {
      return anchor;
    }
  }

  POINT caret{0, 0};
  if (GetCaretPos(&caret)) {
    HWND foreground = GetForegroundWindow();
    if (foreground != nullptr) {
      ClientToScreen(foreground, &caret);
    }
    return caret;
  }

  GetCursorPos(&caret);
  return caret;
}

void TsfTextService::ShowCandidateWindow(ITfContext* context) {
  if (!IsComposing()) {
    HideCandidateWindow();
    return;
  }

  if (EnsureCandidateWindowClass() == 0) {
    return;
  }

  if (candidate_window_ == nullptr) {
    candidate_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                        L"FluentPinyinCandidateWindow",
                                        L"",
                                        WS_POPUP,
                                        CW_USEDEFAULT,
                                        CW_USEDEFAULT,
                                        360,
                                        52,
                                        nullptr,
                                        nullptr,
                                        g_module_instance,
                                        this);
    if (candidate_window_ == nullptr) {
      return;
    }
  }

  const int candidate_count = static_cast<int>(std::max<size_t>(candidates_.size(), 1));
  const int width = 360;
  const int height = 18 + candidate_count * 30;
  POINT caret = CandidateWindowAnchor(context);

  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  int x = caret.x;
  int y = caret.y + 24;
  if (x + width > work_area.right) {
    x = work_area.right - width;
  }
  if (x < work_area.left) {
    x = work_area.left;
  }
  if (y + height > work_area.bottom) {
    y = caret.y - height - 8;
  }
  if (y < work_area.top) {
    y = work_area.top;
  }
  SetWindowPos(candidate_window_,
               HWND_TOPMOST,
               x,
               y,
               width,
               height,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  InvalidateRect(candidate_window_, nullptr, TRUE);
}

void TsfTextService::HideCandidateWindow() {
  if (candidate_window_ != nullptr) {
    ShowWindow(candidate_window_, SW_HIDE);
  }
}

void TsfTextService::DestroyCandidateWindow() {
  if (candidate_window_ != nullptr) {
    DestroyWindow(candidate_window_);
    candidate_window_ = nullptr;
  }
}

void TsfTextService::DrawCandidateWindow(HDC dc) {
  RECT client{};
  GetClientRect(candidate_window_, &client);

  HBRUSH background = CreateSolidBrush(RGB(249, 249, 249));
  FillRect(dc, &client, background);
  DeleteObject(background);

  HPEN border = CreatePen(PS_SOLID, 1, RGB(218, 218, 218));
  HGDIOBJ old_pen = SelectObject(dc, border);
  HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
  RoundRect(dc, client.left, client.top, client.right, client.bottom, 8, 8);
  SelectObject(dc, old_brush);
  SelectObject(dc, old_pen);
  DeleteObject(border);

  HFONT font = CreateFontW(-16,
                           0,
                           0,
                           0,
                           FW_NORMAL,
                           FALSE,
                           FALSE,
                           FALSE,
                           DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE,
                           L"Microsoft YaHei UI");
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  SetBkMode(dc, TRANSPARENT);

  RECT input_rect{14, 8, client.right - 64, 30};
  SetTextColor(dc, RGB(94, 94, 94));
  std::wstring input = AsciiToWide(composition_input_);
  if (candidate_page_index_ > 0) {
    input += L"  ";
    input += std::to_wstring(candidate_page_index_ + 1);
    input += L"/";
  }
  DrawTextW(dc,
            input.c_str(),
            -1,
            &input_rect,
            DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

  int y = 34;
  const size_t count = std::min<size_t>(candidates_.size(), 8);
  for (size_t index = 0; index < count; ++index) {
    RECT row{8, y - 2, client.right - 8, y + 26};
    if (index == 0) {
      HBRUSH selected = CreateSolidBrush(RGB(229, 241, 251));
      FillRect(dc, &row, selected);
      DeleteObject(selected);
    }

    SetTextColor(dc, RGB(96, 96, 96));
    std::wstring number = std::to_wstring(index + 1);
    RECT number_rect{16, y, 40, y + 24};
    DrawTextW(dc, number.c_str(), -1, &number_rect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    SetTextColor(dc, RGB(32, 32, 32));
    RECT text_rect{44, y, client.right - 92, y + 24};
    DrawTextW(dc,
              candidates_[index].text.c_str(),
              -1,
              &text_rect,
              DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    if (!candidates_[index].comment.empty()) {
      SetTextColor(dc, RGB(120, 120, 120));
      RECT comment_rect{client.right - 88, y, client.right - 14, y + 24};
      DrawTextW(dc,
                candidates_[index].comment.c_str(),
                -1,
                &comment_rect,
                DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }

    y += 30;
  }

  if (count == 0) {
    SetTextColor(dc, RGB(120, 120, 120));
    RECT empty_rect{14, 36, client.right - 14, 62};
    DrawTextW(
        dc, L"\u65E0\u5019\u9009", -1, &empty_rect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
  } else {
    std::wstring pager;
    if (has_previous_candidate_page_) {
      pager += L"\u25C0";
    }
    if (has_next_candidate_page_) {
      if (!pager.empty()) {
        pager += L" ";
      }
      pager += L"\u25B6";
    }
    if (!pager.empty()) {
      SetTextColor(dc, RGB(130, 130, 130));
      RECT pager_rect{client.right - 58, 8, client.right - 12, 30};
      DrawTextW(
          dc, pager.c_str(), -1, &pager_rect, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
    }
  }

  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
}

ATOM TsfTextService::EnsureCandidateWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = StaticCandidateWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = L"FluentPinyinCandidateWindow";
  atom = RegisterClassExW(&window_class);
  return atom;
}

LRESULT TsfTextService::CandidateWindowProc(HWND window,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam) {
  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_LBUTTONDOWN: {
      const int y = static_cast<short>(HIWORD(lparam));
      const int index = (y - 32) / 30;
      if (index >= 0 && index < static_cast<int>(std::min<size_t>(candidates_.size(), 8))) {
        CommitCandidateFromMouse(static_cast<size_t>(index));
      }
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(window, &paint);
      DrawCandidateWindow(dc);
      EndPaint(window, &paint);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_NCDESTROY:
      if (candidate_window_ == window) {
        candidate_window_ = nullptr;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticCandidateWindowProc(HWND window,
                                                           UINT message,
                                                           WPARAM wparam,
                                                           LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->CandidateWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace fp::tsf
