#include "ui.h"

#include "applogic.h"
#include "imgui.h"
#include "imfilebrowser.h"
//#define _NJ_INCLUDE_HEADER_ONLY
#include "nanojpeg.c"

#include <type_traits>
#include <vector>
#include <sstream>
#include <optional>
#include <string>
#include <numeric>
#include <tuple>
#include <functional>
#include <chrono>
#include <iomanip>
#include <cstdio>
#include <time.h>
#ifdef __GNUC__
#include <sys/time.h>
#endif
#include <ctime>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int decodePNG(std::vector<unsigned char>& out_image, unsigned long& image_width, unsigned long& image_height, const unsigned char* in_png, size_t in_size, bool convert_to_rgba32 = true);

//-----------------------------------------------------------------------------------

namespace ImGui {

void Spinner(const char* label, float radius, int thickness, const ImU32& color) {

  const ImGuiStyle& style = GetStyle();

  ImVec2 pos = ImGui::GetCursorScreenPos();
  ImVec2 size((radius )*2, (radius + style.FramePadding.y)*2);
  //        const ImRect bb(pos, size);
  //        ItemSize(bb, style.FramePadding.y);
  //        const ImGuiID id = GetID(label);
  //        if (!ItemAdd(bb, id))
  //            return;

  // Render
  GetWindowDrawList()->PathClear();

  int num_segments = 30;
  const float t = ImGui::GetTime();
  int start = abs(sinf(t*1.8f)*(num_segments-5));

  const float a_min = M_PI*2.0f * ((float)start) / (float)num_segments;
  const float a_max = M_PI*2.0f * ((float)num_segments-3) / (float)num_segments;

  const ImVec2 centre = ImVec2(pos.x+radius, pos.y+radius+style.FramePadding.y);

  for (int i = 0; i < num_segments; i++) {
    const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
    GetWindowDrawList()->PathLineTo(ImVec2(centre.x + cosf(a+t*8) * radius,
                                           centre.y + sinf(a+t*8) * radius));
  }

  GetWindowDrawList()->PathStroke(color, false, thickness);
}

}
namespace
{
//-----------------------------------------------------------------------------------
class TextureInfo
{
public:
  operator bool()const{return _id;}
/*
  ~TextureInfo()
  {
    clear();
  }*/
  void clear()
  {
    if(_id)
    {
      imgui_app_destroyImage(_id);
      *this = TextureInfo{};
    }
  }
  void loadIfChanged(Blob const& img, Blob& lastImg)
  {
    static constexpr int N = 128;
    if(img.size()!=lastImg.size() ||
       (img.size()>N && lastImg.size()>N &&
        !std::equal(img.cbegin(), img.cbegin()+N, lastImg.cbegin())))
    {
      load(img);
      lastImg = img;
    }
  }
  void load(Blob const& img)
  {
    int const size = img.size();
    const void* data = img.data();
    auto oldId = _id;
    _id = nullptr;
    if(oldId)
    {
      imgui_app_destroyImage(_id);
    }
    std::vector<unsigned char> out_image;
    unsigned long image_width, image_height;
    if(0==decodePNG(out_image,image_width, image_height, (unsigned char*)data, size))
    {
      _id = imgui_app_loadImageRGBA8(out_image.data(), image_width, image_height);
      _w = (int)image_width;
      _h = (int)image_height;
    }
    else
    {
      nj_result_t res = njDecode(data, size);

      if(NJ_OK == res)
      {
        _w = njGetWidth();
        _h = njGetHeight();
        unsigned char* buffer = njGetImage();
        std::vector<unsigned char> rgba(_w*_h*4);
        if(njIsColor())
        {
          for(int i=0;i<rgba.size();i+=4)
          {
            rgba[i+0]=*(buffer++);
            rgba[i+1]=*(buffer++);
            rgba[i+2]=*(buffer++);
            rgba[i+3]=255;
          }
        }
        else
        {
          for(int i=0;i<rgba.size();i+=4)
          {
            auto c = *(buffer++);
            static constexpr auto R=255*0.299;
            static constexpr auto G=255*0.587;
            static constexpr auto B=255*0.114;
            rgba[i+0]=c*R;
            rgba[i+1]=c*G;
            rgba[i+2]=c*B;
            rgba[i+3]=255;
          }
        }
        _id = imgui_app_loadImageRGBA8(rgba.data(), _w, _h);
      }
      else
      {
        *this = TextureInfo{};
      }
    }
  }
  bool show(int displayW = DEFAULT_DISPLAY_SIZE,
            int displayH = DEFAULT_DISPLAY_SIZE) const
  {
    if(!_id)
    {
      return false;
    }
    ImGui::Image(_id, ImVec2(displayW, displayH));
    return true;
  }
  bool showTooltip(int region_sz = DEFAULT_DISPLAY_SIZE) const
  {
    if(!_id)
    {
      return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
    float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
    float zoom = 1.0f;
    if (region_x < 0.0f) { region_x = 0.0f; }
    else if (region_x > _w - region_sz) { region_x = _w - region_sz; }
    if (region_y < 0.0f) { region_y = 0.0f; }
    else if (region_y > _h - region_sz) { region_y = _h - region_sz; }
    ImVec2 uv0 = ImVec2((region_x) / _w, (region_y) / _h);
    ImVec2 uv1 = ImVec2((region_x + region_sz) / _w, (region_y + region_sz) / _h);
    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
    ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.f); // No Border
    ImGui::Image(_id, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, tint_col, border_col);
    return true;
  }
private:
  static constexpr int DEFAULT_DISPLAY_SIZE=64;

  ImTextureID _id=nullptr;
  int _w=0, _h=0;
};
//-----------------------------------------------------------------------------------
void podcastToolip(AppLogic& app, int const i)
{
  if (ImGui::IsItemHovered())
  {
    static TextureInfo tex;
    static Blob lastImg;

    auto const& p{app.podcast(i)};
    tex.loadIfChanged(p.image_blob, lastImg);
    ImGui::BeginTooltip();
    tex.showTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(p.summary.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}
//-----------------------------------------------------------------------------------
void showTable(AppLogic& app)
{

  ImVec2 outer_size = ImVec2(0.0f, 0.0f);
  static constexpr ImGuiTableFlags flags =
      ImGuiTableFlags_Borders |
      ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_ScrollY |
      ImGuiTableFlags_Sortable |
      ImGuiTableFlags_SortTristate;
  if (ImGui::BeginTable("table2", 4, flags, outer_size))
  {
    enum ColTag{ColTag_Action, ColTag_Title, ColTag_Length, ColTag_Date};
    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
    ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_NoSort|ImGuiTableColumnFlags_WidthFixed, 0.f, ColTag_Action);
    ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch, 0.f, ColTag_Title);
    ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed|ImGuiTableColumnFlags_PreferSortDescending, 0.f, ColTag_Length);
    ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 0.f, ColTag_Date);

    // Instead of calling ImGui::TableHeadersRow(); we'll submit custom headers ourselves
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    auto customHeader=[&](int const column ,auto && custom){
      ImGui::TableNextColumn();
      const char* column_name = ImGui::TableGetColumnName(column); // Retrieve name passed to TableSetupColumn()
      ImGui::PushID(column);
      if constexpr (!std::is_same_v<bool, std::decay_t<decltype(custom)>>)
      {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        custom();
        ImGui::PopStyleVar();
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
      }
      ImGui::TableHeader(column_name);
      ImGui::PopID();
    };
    customHeader(0, false);
    customHeader(1, false
                 /*[&]{
      static char buf2[64] = "";
      if(ImGui::InputText("",     buf2, 64, ImGuiInputTextFlags_EnterReturnsTrue))
      {
        //app.setNameFilter(buff2);
      }
    }*/);
    customHeader(2, false);
    customHeader(3, false);

    if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
    {
      if (sorts_specs->SpecsDirty)
      {
        if(sorts_specs->Specs)
        {
          std::optional<Storage::SortingOption> sortingDir;
          switch(sorts_specs->Specs->SortDirection)
          {
          case ImGuiSortDirection_None:
            break;
          case ImGuiSortDirection_Ascending:
            sortingDir=Storage::SortingOption::ASCENDING;
            break;
          case ImGuiSortDirection_Descending:
            sortingDir=Storage::SortingOption::DESCENDING;
            break;
          }

          if(sortingDir)
          {
            if(sorts_specs->Specs->ColumnUserID == ColTag_Title)
            {
              app.setShowSorting(&MediaViewCols::title, *sortingDir);
            }
            else if(sorts_specs->Specs->ColumnUserID == ColTag_Date)
            {
              app.setShowSorting(&MediaViewCols::date, *sortingDir);
            }
            else if(sorts_specs->Specs->ColumnUserID == ColTag_Length)
            {
              app.setShowSorting(&MediaViewCols::duration, *sortingDir);
            }
          }
          else
          {
            app.setShowSorting<int>(nullptr);
          }
        }
        else
        {
          app.setShowSorting<int>(nullptr);
        }
        sorts_specs->SpecsDirty = false;
      }
    }


    if(app.showCount()>0)
    {
      ImGuiListClipper clipper;
      clipper.Begin(app.showCount());
      while (clipper.Step())
      {
        std::vector<MediaViewCols>const& shows =
            (clipper.DisplayStart == 0 && clipper.DisplayEnd == 1)
            ? app.showsIn0to1RankRange()
            : app.showsInRankRange(clipper.DisplayStart, clipper.DisplayEnd-clipper.DisplayStart);
        for (int row = clipper.DisplayStart, i=0; row < clipper.DisplayEnd; ++row,++i)
        {
          if(i>=shows.size())
          {
            break;
          }
          MediaViewCols const& show = shows[i];
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::SmallButton("<");
          if (ImGui::IsItemClicked())
          {
            app.selectShowRange(0, row, ImGui::GetIO().KeyCtrl);
          }
          ImGui::SameLine();
          ImGui::SmallButton(">");
          if (ImGui::IsItemClicked())
          {
            app.selectShowRange(row, app.showCount()-1, ImGui::GetIO().KeyCtrl);
          }

          ImGui::TableNextColumn();
          bool rowSelected = app.isShowRankSelected(row);
          if(ImGui::Selectable(show.title.c_str(), &rowSelected, ImGuiSelectableFlags_SpanAllColumns))
          {
            app.showSelection(row, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift);
          }
          ImGui::SameLine();
          ImGui::TextDisabled("...");
          if (ImGui::IsItemHovered())
          {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(show.summary.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
          }
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(show.durationStr().c_str());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(show.dateStr().c_str());
        }
      }
      clipper.End();
    }

    ImGui::EndTable();
  }
}
//-----------------------------------------------------------------------------------
enum class PodcastPropertyAction{ADD, UPDATE};
void displayAddPodcastWindow(AppLogic& app, bool& showAddPodcast,
                             std::optional<PodcastPropertyAction>& action,
                             std::optional<PodcastCols> const& toUpdate = std::nullopt)

{
  static std::optional<int> updatePodcastId;
  //editable info:
  static constexpr int urlMaxSize=1024;
  static char url[urlMaxSize];
  static constexpr int nameMaxSize=1024;
  static char name[nameMaxSize];
  static constexpr int patternMaxSize=1024;
  static char pattern[patternMaxSize];
  static constexpr int pathMaxSize=1024;
  static char path[pathMaxSize];
  //read only info:
  static std::string description;
  static Blob image;
  static TextureInfo imageTex;
  static std::string imageUrl;
  static std::vector<std::string> showListPreview;

  static ImGui::FileBrowser fileDialog(ImGuiFileBrowserFlags_SelectDirectory|ImGuiFileBrowserFlags_CreateNewDir);

  // (optional) set browser properties
  fileDialog.SetTitle("Select download destination folder");

  static const char* popupName = "Podcast setup";
  if(action)
  {
    ImGui::OpenPopup(popupName);
    switch(*action)
    {
    case PodcastPropertyAction::ADD:
      updatePodcastId.reset();
      name[0]=0;
      description.clear();
      image.clear();
      imageUrl.clear();
      imageTex.clear();
      showListPreview.clear();
      break;
    case PodcastPropertyAction::UPDATE:
      if(toUpdate)
      {
        updatePodcastId = toUpdate->id;
        strcpy(url, toUpdate->link.c_str());
        strcpy(name, toUpdate->title.c_str());
        strcpy(pattern, toUpdate->pattern.c_str());
        strcpy(path, toUpdate->target.c_str());
        description = toUpdate->summary;
        imageUrl.clear();
        image = toUpdate->image_blob;
        imageTex.load(image);
        showListPreview.clear();
      }
      break;
    }
    action.reset();
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);

  if (!ImGui::BeginPopupModal(popupName,
                    &showAddPodcast,
                    ImGuiWindowFlags_NoCollapse|
                    ImGuiWindowFlags_NoScrollbar|
                    ImGuiWindowFlags_NoScrollWithMouse))
  {
    return;
  }
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
  if (ImGui::BeginTable("##addPodcastProperties", 2, ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Prop", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    {
      ImGui::TableSetColumnIndex(0);
      {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("URL");
      }
      ImGui::TableSetColumnIndex(1);
      {
        ImGui::SetNextItemWidth(-50);
        ImGui::InputText("##url", url, urlMaxSize);
        ImGui::SameLine();
        if(ImGui::Button("Get"))
        {
          std::optional<PodcastCols> podcast;
          showListPreview.clear();
          std::tie(podcast, showListPreview) = app.queryPodcast(url);
          if(podcast)
          {
            strcpy(name, podcast->title.c_str());
            description = podcast->summary;
            imageUrl = podcast->image_url;
            image = podcast->image_blob;
            imageTex.load(image);
          }
        }
      }
      ImGui::NextColumn();
    }

    ImGui::TableNextRow();
    {
      ImGui::TableSetColumnIndex(0);
      {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Target folder");
      }
      ImGui::TableSetColumnIndex(1);
      {
        ImGui::SetNextItemWidth(-50);
        ImGui::InputText("##target", path, pathMaxSize);
        ImGui::SameLine();
        if(ImGui::Button("..."))
        {
          fileDialog.SetPwd(std::string(path));
          fileDialog.Open();
        }
      }
      ImGui::NextColumn();
    }

    ImGui::TableNextRow();
    {
      ImGui::TableSetColumnIndex(0);
      {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Title");
      }
      ImGui::TableSetColumnIndex(1);
      {
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##name", name, nameMaxSize);
      }
      ImGui::NextColumn();
    }
    ImGui::TableNextRow();
    {
      ImGui::TableSetColumnIndex(0);
      {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Filenames");
      }
      ImGui::TableSetColumnIndex(1);
      {
        ImGui::SetNextItemWidth(-75);
        ImGui::InputText("##pattern", pattern, patternMaxSize);
        if (ImGui::IsItemHovered())
        {
          std::string patternPreview =
              app.computeBaseFilename(pattern, "Show sample title",
                                      std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now()
                                        - std::chrono::system_clock::time_point{}).count());

          ImGui::BeginTooltip();
          ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
          ImGui::TextDisabled("%s", "preview: ");
          ImGui::SameLine();
          ImGui::TextUnformatted(patternPreview.c_str());
          ImGui::TextDisabled("available variables: %s", "{date}, {title}\nexample: {date}-ThePodcast-{title}");
          ImGui::PopTextWrapPos();
          ImGui::EndTooltip();
        }
        ImGui::SameLine();
        if(ImGui::Button("Default"))
        {
          strcpy(pattern, "{date}-{title}");
        }
      }
      ImGui::NextColumn();
    }

    ImGui::TableNextRow();
    {
      ImGui::TableSetColumnIndex(0);
      {
        ImGui::AlignTextToFramePadding();
        if(!imageTex.show())
        {
          ImGui::Text("Description");
        }
      }
      ImGui::TableSetColumnIndex(1);
      {
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SetNextItemWidth(5*ImGui::GetTextLineHeight());
        ImGui::TextWrapped("%s", description.c_str());
      }
      ImGui::NextColumn();
    }

    ImGui::EndTable();
  }
  if (ImGui::BeginListBox("##showPreviewList", ImVec2(-FLT_MIN, -ImGui::GetFrameHeightWithSpacing())))
  {
      for (std::string const& title:showListPreview)
      {
        ImGui::TextUnformatted(title.c_str());
      }
      ImGui::EndListBox();
  }
  ImGui::PopStyleVar();
  if(ImGui::Button(updatePodcastId?"Update podcast settings":"Add this podcast"))
  {
    PodcastCols podcast;
    podcast.id = updatePodcastId?*updatePodcastId:0;
    podcast.link = url;
    podcast.title = name;
    podcast.pattern = pattern;
    podcast.target = path;
    podcast.summary = description;
    podcast.image_url = imageUrl;
    podcast.image_blob = image;
    if(app.insertPodcast(podcast,
                         updatePodcastId?AppLogic::InsertPodcastMode::UPDATE:AppLogic::InsertPodcastMode::ADD))
    {
      showAddPodcast = false;
    }
  }

  fileDialog.Display();
  if(fileDialog.HasSelected())
  {
    strcpy(path, fileDialog.GetSelected().string().c_str());
  }

  ImGui::EndPopup();

}
//-----------------------------------------------------------------------------------
void frame(AppLogic& app)
{
  static bool showAddPodcast = false;
  std::optional<PodcastPropertyAction> podcastPropertyAction;
  std::optional<PodcastCols> podcastToEdit;

  const float TEXT_BASE_HEIGHT = ImGui::GetFrameHeightWithSpacing();
#ifndef NDEBUG
  static bool demo = false;
  if (ImGui::BeginMainMenuBar())
  {
      if (ImGui::MenuItem("Demo")) {demo = !demo;}

    ImGui::EndMainMenuBar();
  }
  if(demo)
  {
    ImGui::ShowDemoWindow();
    return;
  }
#endif
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);

  static constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                            ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoSavedSettings |
                                            ImGuiWindowFlags_NoCollapse |
                                            ImGuiWindowFlags_NoBringToFrontOnFocus |
                                            ImGuiWindowFlags_NoScrollbar |
                                            ImGuiWindowFlags_NoScrollWithMouse;
  static bool open = true;
  if(ImGui::Begin("Main", &open, flags))
  {
    // Leave room for 1 line below us
    ImGui::BeginChild("main view", ImVec2(0, -TEXT_BASE_HEIGHT),
                      false,
                      ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse);
    {

      ImVec2 outer_size0 = ImVec2(0.0f, TEXT_BASE_HEIGHT * 20);
      static constexpr ImGuiTableFlags flags0 = ImGuiTableFlags_Resizable;
      if (ImGui::BeginTable("table2", 2, flags0, outer_size0))
      {

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        std::optional<std::optional<int>> newPodcastNumber;
        std::optional<std::optional<AppLogic::MediaStatus>> newPodcastFilter;
        // Left
        {
          if (ImGui::BeginListBox("##PodcastsList",ImVec2(-FLT_MIN, -FLT_MIN)))
          {
            if(ImGui::Selectable("Add podcast..."))
            {
              podcastPropertyAction = PodcastPropertyAction::ADD;
              podcastToEdit.reset();
            }
            if (ImGui::Selectable("All",  app.noPodcastFilter(),
                                  ImGuiSelectableFlags_AllowItemOverlap |
                                  (app.isBusy()?ImGuiSelectableFlags_Disabled:0)))
            {
              ImGui::SetItemDefaultFocus();
              newPodcastNumber.emplace();
              newPodcastNumber->reset();
            }
            for (int i = 0; i < app.podcastCount(); i++)
            {
              //ensure unique popup for each item
              ImGui::PushID(i);
              static const char* popupId = "podcastPopup";
              if (ImGui::Selectable(app.podcastTitle(i),  app.isCurrentPodcast(i),
                                    ImGuiSelectableFlags_AllowItemOverlap |
                                    (app.isBusy()?ImGuiSelectableFlags_Disabled:0)))
              {
                ImGui::SetItemDefaultFocus();
                newPodcastNumber.emplace();
                newPodcastNumber->emplace(i);
                ImGui::OpenPopupOnItemClick(popupId, ImGuiPopupFlags_MouseButtonRight);
              }
              //cannot call "OpenPopup" from a context menu
              bool deleteConfirmation=false;
              if(ImGui::BeginPopupContextItem(popupId))
              {
                ImGui::TextDisabled("%s", app.podcastTitle(i));
                if (ImGui::MenuItem("Settings..."))
                {
                  podcastPropertyAction = PodcastPropertyAction::UPDATE;
                  podcastToEdit.emplace(app.podcast(i));
                }
                if (ImGui::MenuItem("Refresh"))
                {
                  app.refreshPodcastAtIndex(i);
                }
                if (ImGui::MenuItem("Delete"))
                {
                  deleteConfirmation = true;
                }
                ImGui::EndPopup();
              }
              if(deleteConfirmation)
              {
                ImGui::OpenPopup("Delete?");
              }
              if (ImGui::BeginPopupModal("Delete?"))
              {
                ImGui::TextUnformatted(app.podcastTitle(i));
                if (ImGui::Button("Confirm delete"))
                {
                  app.deletePodcastAtIndex(i);
                  ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                  ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
              }
              ImGui::SameLine();
              ImGui::SmallButton("...");
              if (ImGui::IsItemClicked())
              {
                if (!ImGui::IsPopupOpen(popupId))
                {
                  ImGui::OpenPopup(popupId);
                }
              }


              podcastToolip(app, i);


              ImGui::PopID();

            }
            ImGui::EndListBox();
          }
        }

        ImGui::TableNextColumn();

        // Right
        if(app.isBusy())
        {
          const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
          ImGui::Spinner("##spinner", 80, 2, col);
        }
        else
        {
          ImGui::BeginGroup();
          ImGui::BeginChild("##SeletedPodcastShows", ImVec2(0, -TEXT_BASE_HEIGHT)); // Leave room for 1 line below us

          ImGui::TextUnformatted("Status filter:");
          ImGui::SameLine();

          ImGui::BeginGroup();
          {
            auto itemForStatus = [&](const char* label, std::optional<AppLogic::MediaStatus> const& status)
            {

              ImGui::PushID(status?*status:1000);
              if (ImGui::Selectable(label,
                                    app.isStatusActive(status),
                                    ImGuiSelectableFlags_None,
                                    ImGui::CalcTextSize(label, NULL, true)))
              {
                newPodcastFilter.emplace(status);
              }
              ImGui::PopID();
              ImGui::SameLine();
            };
            itemForStatus("New", AppLogic::MediaStatus::New);
            itemForStatus("Download queue", AppLogic::MediaStatus::Queued);
            itemForStatus("Skipped", AppLogic::MediaStatus::Skipped);
            itemForStatus("Done", AppLogic::MediaStatus::Done);
            itemForStatus("All", std::nullopt);
            ImGui::EndGroup();
          }
          if(newPodcastNumber || newPodcastFilter)
          {
            app.setCurrentPodcastRowIndex(newPodcastNumber,newPodcastFilter);
          }
          showTable(app);

          ImGui::EndChild();

          if (app.isStatusActive(AppLogic::MediaStatus::New) )
          {
            if( ImGui::Button("Update"))
            {
              app.refreshCurrentPodcast();
            }
            ImGui::SameLine();
          }
          if (app.isStatusActive(AppLogic::MediaStatus::Queued) )
          {
            if(app.isDownloading())
            {
              ImGui::Text("Downloading %c", "|/-\\"[(int)(ImGui::GetTime() / 0.1f) & 3]);
            }
            else
            {
              if( ImGui::Button("Start downloading ALL"))
              {
                app.startDownload();
              }
            }
            ImGui::SameLine();
          }
          if(app.anySelection())
          {
            if (!app.isStatusActive(AppLogic::MediaStatus::New))
            {
              if(ImGui::Button("Reset as new"))
              {
                app.setSelectedShowsStatus(Status::NEW);
              }
              ImGui::SameLine();
            }
            if (!app.isStatusActive(AppLogic::MediaStatus::Skipped))
            {
              if(ImGui::Button("Skip selection"))
              {
                app.setSelectedShowsStatus(Status::SKIPPED);
              }
              ImGui::SameLine();
            }
            if (!app.isStatusActive(AppLogic::MediaStatus::Queued) )
            {
              if( ImGui::Button("Queue selection to download"))
              {
                app.setSelectedShowsStatus(Status::QUEUED);
              }
              ImGui::SameLine();
            }
          }
          ImGui::EndGroup();
        }
        ImGui::EndTable();
      }

      ImGui::EndChild();
    }
    if(app.isBusy())
    {
      ImGui::Text("Updating %c", "|/-\\"[(int)(ImGui::GetTime() / 0.1f) & 3]);
    }
    else if(app.isDownloading())
    {
      ImGui::SmallButton("Abort");
      if (ImGui::IsItemClicked())
      {
        app.abortDownload();
      }
      ImGui::SameLine();
      auto const progress = app.getCurrentDownloadProgress();
      ImGui::Text("%d/%d: %s %c",
                  progress.currentFile,
                  progress.totalFiles,
                  progress.currentLabel.c_str(),
                  "|/-\\"[(int)(ImGui::GetTime() / 0.1f) & 3]);
      ImGui::SameLine();
      ImGui::ProgressBar(progress.currentProgress/float(progress.currentTotal),ImVec2(-100, 0));
      ImGui::SameLine();
      static constexpr auto oneMb=1024*1024;
      ImGui::Text(progress.bytesPerSec > oneMb ? "%.2f Mb/s":"%.0f kb/s",
                  progress.bytesPerSec/(progress.bytesPerSec > oneMb ? (float)oneMb:1024.f));
    }
    else if(const std::string* err = app.lastError())
    {
      ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Error: %s", err->c_str());
    }
    ImGui::End();

  }
  if (podcastPropertyAction)
  {
    showAddPodcast=true;
  }
  if (showAddPodcast)
  {
    displayAddPodcastWindow(app, showAddPodcast, podcastPropertyAction, podcastToEdit);
  }
}

void frame()
{
  static AppLogic app("castapod.db3");
  frame(app);
}
}
void runUI()
{
  njInit();

  // when ready start the UI (this will not return until the app finishes)
  int imguiConfigFlags = 0;
#ifdef IMGUI_HAS_DOCK
  imguiConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
  imgui_app(frame, "Podcast Downloader", 1024, 768, imguiConfigFlags);
}
