#!/usr/bin/env python3
"""Render actual compact UI branches with a host OLED adapter and repository fonts.
Requires clang++, Pillow and the PlatformIO OLED dependency (installed by a build).
This checks raster bounds and layout; it does not emulate hardware input or I2C.
"""
from pathlib import Path
import re, subprocess, sys, shutil
root=Path(__file__).resolve().parents[2]
base=root/'src/graphics/hermesx_ui'
out=Path(sys.argv[1] if len(sys.argv)>1 else '/tmp/hermesone-ui-preview').resolve()
out.mkdir(parents=True,exist_ok=True)
for name in ['Arduino.h','OLEDDisplay.h','configuration.h','graphics/TFTDisplay.h']:
 target=out/name
 target.parent.mkdir(parents=True,exist_ok=True)
 shutil.copy2(Path(__file__).parent/name,target)
def function(src,name):
 start=src.index(name);start=src.rfind('\n',0,start)+1
 brace=src.index('{',start);depth=1;i=brace+1
 while depth:
  if src[i]=='{':depth+=1
  if src[i]=='}':depth-=1
  i+=1
 return src[start:i]
src=(base/'HermesXFastSetupUiRenderer.cpp').read_text()
funcs=[function(src,'void HermesXFastSetupUiRenderer::'+n+'(') for n in ['drawHeader','drawToast','drawKeyboardPage','drawList']]
list_helper='template <typename LabelProvider>\n'+function(src,'void drawListItems(')
for name in ['drawUpdateProgressPage','drawUrlUpdateFlowPage']:
 full=function(src,'void HermesXFastSetupUiRenderer::'+name+'(')
 branch=function(full,'    if (HermesXCompactUi::applies(display))')
 funcs.append(full[:full.index('{')+1]+'\n'+branch+'\n}')
helper=src[src.index('constexpr int16_t HeaderHeight'):src.index('#if defined(ST7735_CS)')]
composer=(base/'HermesXDirectMessageComposer.cpp').read_text();keys=composer[composer.index('const char *kKeyRowsUpper'):composer.index('bool matches(')]
msg=function((base/'HermesXMessageUiRenderer.cpp').read_text(),'void HermesXMessageUiRenderer::drawComposerCandidates(')
home=(base/'HermesXHomeUiRenderer.cpp').read_text()
mascot=home[home.index('constexpr int16_t SittingDogWidth'):home.index('uint8_t quoteIndex')]
notification_actions=function(home,'void drawNotificationActions(')
dog=function(home,'void HermesXHomeUiRenderer::drawMonochromeSittingDog(')
parcel=function(home,'void HermesXHomeUiRenderer::drawHermesOneParcelNotification(')
pre='''#include "Arduino.h"
#include "OLEDDisplay.h"
#include "graphics/hermesx_ui/HermesXCompactUi.h"
#include "graphics/hermesx_ui/HermesXEmergencyConfirmUiRenderer.h"
#include "graphics/hermesx_ui/HermesXLowMemoryUiRenderer.h"
#include "graphics/hermesx_ui/HermesXRotaryLockUiRenderer.h"
#include <vector>
#include <cstring>
namespace graphics {
'''
def decl(f,cls):return f[:f.index('{')].replace('void '+cls+'::','static void ').strip()+';'
pre+='class HermesXFastSetupUiRenderer {public:\n'+'\n'.join(decl(f,'HermesXFastSetupUiRenderer') for f in funcs)+'\n};\n'
pre+='class HermesXMessageUiRenderer {public:\n'+decl(msg,'HermesXMessageUiRenderer')+'\n};\n'
pre+='enum class HermesXParcelAnimationMode : uint8_t {Outgoing, Incoming, Failed};\n'
dog_decl=decl(dog,'HermesXHomeUiRenderer').replace('bool mirror);','bool mirror = false);')
parcel_decl=decl(parcel,'HermesXHomeUiRenderer').replace('const char *message);','const char *message = nullptr);')
pre+='class HermesXHomeUiRenderer {public:\n'+dog_decl+'\n'+parcel_decl+'\n};\n'
pre+='namespace {\n'+mascot+'\n'+notification_actions+'}\n'+dog+'\n'+parcel+'\n'
pre+=helper+'\n'+keys+'\n'+list_helper+'\n'+'\n'.join(funcs)+'\n'+msg+'\n}\n'
stealth_source=(root/'src/graphics/Screen.cpp').read_text()
stealth_source=stealth_source[stealth_source.index('    const bool yesArmed = (remainMs == 0);'):]
stealth=function(stealth_source,'    if (HermesXCompactUi::applies(display))')
pre+='void stealthPreview(OLEDDisplay *display, uint32_t remainMs) { using namespace graphics; int width=128; bool yesArmed=remainMs==0; int hermesActionStealthConfirmSelected=0;\n'+stealth+'\n}\n'

navigation=function(composer,'    if (navDir != 0) {')
pre+='namespace graphics { enum class HermesXComposerAction {Refresh};\n'
pre+='HermesXComposerAction moveKeyboard(uint8_t &keyRow_, uint8_t &keyCol_, const uint8_t *lengths, int navDir) { auto keyRowLengths=[&](){return lengths;};\n'+navigation+'\nreturn HermesXComposerAction::Refresh; } }\n'

pre+='''
using namespace graphics;
int main(){
 String toast=""; uint32_t until=0;
 const char *const (*keysets[])[10]={kKeyRowsUpper,kKeyRowsLower,kKeyRowsBopomofo};
 int checked=0;
 for(int mode=0;mode<3;mode++)for(int row=0;row<5;row++){
  const uint8_t *lengths=mode==2?kRowLengthsBopomofo:kRowLengthsEnglish;
  for(int col=0;col<lengths[row];col++){
   OLEDDisplay d;
   HermesXFastSetupUiRenderer::drawKeyboardPage(&d,128,64,"MSG abcd",u8"測試中文注音輸入",keysets[mode],lengths,5,row,col,toast,until);
   assert(!d.outside);checked++;
   if((row==0&&col==0)||(row==3&&col==8)||(row==4&&col==lengths[row]-1)){
    char name[64];snprintf(name,sizeof(name),"keyboard_%d_%d_%d",mode,row,col);d.save(name);
   }
  }
 }
 // Run the current composer navigation block, not a second implementation.
 for(const uint8_t *lengths : {kRowLengthsEnglish,kRowLengthsBopomofo}) {
  uint8_t row=0,col=0;
  for(int i=0;i<9;i++)moveKeyboard(row,col,lengths,1);
  assert(row==0 && col==9); // upper-right, still page one
  moveKeyboard(row,col,lengths,1);assert(row==1 && col==0);
  moveKeyboard(row,col,lengths,-1);assert(row==0 && col==9);
  moveKeyboard(row,col,lengths,1);
  for(int i=0;i<9;i++)moveKeyboard(row,col,lengths,1);
  assert(row==1 && col==9); // lower-right is the next-page boundary
  moveKeyboard(row,col,lengths,1);assert(row==2 && col==0);
  moveKeyboard(row,col,lengths,-1);assert(row==1 && col==9);
  row=0;col=0;moveKeyboard(row,col,lengths,-1);
  assert(row==4 && col==lengths[4]-1);
  moveKeyboard(row,col,lengths,1);assert(row==0 && col==0);
 }
 OLEDDisplay d; HermesXEmergencyConfirmUiState em;em.visible=true;em.remainingSeconds=5;
 HermesXEmergencyConfirmUiRenderer::draw(&d,em);d.save("EM");
 d=OLEDDisplay();HermesXRotaryLockUiState rot;rot.visible=true;rot.selectedLocked=true;
 HermesXRotaryLockUiRenderer::draw(&d,rot);d.save("lock");
 d=OLEDDisplay();HermesXLowMemoryUiState low;low.visible=true;low.triggerFree=12345;low.triggerLargest=7890;
 HermesXLowMemoryUiRenderer::draw(&d,low,true);d.save("heap");
 uint16_t scroll=0,max=0;
 const char *body=u8"這是一則較長的警示文字，用來確認中文字不會碰到標題或按鈕。最後一行也必須完整顯示。";
 d=OLEDDisplay();HermesXCompactUi::scrollable(&d,u8"設定詳情",body,scroll,max);d.save("detail_first");
 assert(max>0);int initial=max;
 for(int i=0;i<20;i++){scroll+=13;d=OLEDDisplay();HermesXCompactUi::scrollable(&d,u8"設定詳情",body,scroll,max);assert(!d.outside);assert(scroll%14==0);assert(scroll<=max);}
 assert(scroll==initial);d.save("detail_last");
 d=OLEDDisplay();toast=u8"請先輸入訊息再送出";until=1200;
 HermesXFastSetupUiRenderer::drawKeyboardPage(&d,128,64,"MSG abcd","",kKeyRowsUpper,kRowLengthsEnglish,5,4,4,toast,until);d.save("toast");
 d=OLEDDisplay();HermesXMessageUiRenderer::drawComposerCandidates(&d,128,64,u8"注音輸入預覽長文字",{u8"你好",u8"中文",u8"測試很長的中文候選詞"},3);d.save("candidates");
 const char *menuItems[]={u8"返回",u8"第一項",u8"第二項",u8"第三項",u8"第四項"};
 d=OLEDDisplay();HermesXFastSetupUiRenderer::drawList(&d,128,64,"ONLINE",menuItems,5,3,0);d.save("list_four_rows");
 bool bottomRowInk=false;for(int py=52;py<64;py++)for(int px=0;px<128;px++)bottomRowInk|=d.pixels[py*128+px]!=0;
 assert(bottomRowInk);
 d=OLEDDisplay();
 for(int row=0;row<4;row++)HermesXTextLayout::drawMixedSingleLineBounded(&d,3,16+row*12,121,u8"MSG跑馬燈很長但不可覆蓋下一列ABCDEFGHIJK",12,12);
 assert(!d.outside);d.save("msg_marquee_four_rows");
 d=OLEDDisplay();stealthPreview(&d,3000);d.save("stealth_wait");
 d=OLEDDisplay();stealthPreview(&d,0);d.save("stealth_ready");
 toast="";until=0;
 d=OLEDDisplay();HermesXFastSetupUiRenderer::drawUpdateProgressPage(&d,128,64,u8"更新失敗",u8"無法連線",0,"","",u8"請檢查連線後重試",false,2,u8"重試",u8"返回",1,toast,until);d.save("update_error");
 d=OLEDDisplay();HermesXFastSetupUiRenderer::drawUrlUpdateFlowPage(&d,128,64,u8"更新失敗","",0,u8"請檢查連線後重試",u8"重試",0,toast,until);d.save("url_error");
 const uint32_t parcelTimes[]={0,450,900,1350,1800};
 for(auto t:parcelTimes){
  d=OLEDDisplay();HermesXHomeUiRenderer::drawHermesOneParcelNotification(&d,128,64,HermesXParcelAnimationMode::Outgoing,t,false,0);char n[40];snprintf(n,sizeof(n),"parcel_out_%04u",t);d.save(n);
  d=OLEDDisplay();HermesXHomeUiRenderer::drawHermesOneParcelNotification(&d,128,64,HermesXParcelAnimationMode::Incoming,t,true,0,t>=1800?u8"收到訊息測試":"");snprintf(n,sizeof(n),"parcel_in_%04u",t);d.save(n);
 }
 for(auto t:{3000U,5000U,7500U}){d=OLEDDisplay();HermesXHomeUiRenderer::drawHermesOneParcelNotification(&d,128,64,HermesXParcelAnimationMode::Incoming,t,true,0,u8"第十二三四五六七八九十字");char n[40];snprintf(n,sizeof(n),"parcel_text_%04u",t);d.save(n);}
 const uint32_t failTimes[]={0,500,850,1200,1550};
 for(auto t:failTimes){d=OLEDDisplay();HermesXHomeUiRenderer::drawHermesOneParcelNotification(&d,128,64,HermesXParcelAnimationMode::Failed,t,false,0);char n[40];snprintf(n,sizeof(n),"parcel_fail_%04u",t);d.save(n);}
 printf("Verified %d keyboard cursor positions; four-row lists, page/row navigation boundaries, compact dialogs, full-line scrolling, toast, candidates, and 18 parcel-animation keyframes.\\n",checked);
}
'''
(lambda audits: [(_ for _ in ()).throw(AssertionError(f'{path} compact list audit failed'))
                 for path,needles in audits.items()
                 if any(needle not in (root/path).read_text() for needle in needles)])({
 'src/graphics/hermesx_ui/HermesXFastSetupUiModel.h':['VisibleRows = 4'],
 'src/graphics/hermesx_ui/HermesXFastSetupUiRenderer.cpp':['HermesOneRowHeight = 12','HermesOneVisibleRows = 4'],
 'src/graphics/hermesx_ui/HermesXMessageUiRenderer.cpp':['std::min<int8_t>(visibleRows, 4)','drawMixedSingleLineBounded(\n            display, lineX, rowY, lineWidth, visibleLine','snippet, sizeof(snippet), 20','> 20U','memcpy(snippet + used, "...", 4)'],
 'src/graphics/hermesx_ui/HermesXHomeUiRenderer.cpp':['true, true);','drawRect(54, 13, width - 55, 18)','width - 61, message, 13'],
 'src/graphics/hermesx_ui/HermesXTraceRouteUiRenderer.cpp':['std::min<int8_t>(visibleRows, 4)'],
 'src/graphics/Screen.cpp':['gOnlineNodeState.listCursor, rows, 4','gFinderNodeState.listCursor, rows, 4','kIncomingTextPopupMs = 8000'],
})
(out/'preview.cpp').write_text(pre)
lib=root/'.pio/libdeps/nrf52_promicro_diy_tcxo_hermesx/ESP8266 and ESP32 OLED driver for SSD1306 displays/src'
sources=[out/'preview.cpp',base/'HermesXTextLayout.cpp',root/'src/graphics/fonts/HermesX_zh/HermesX_CN12.cpp',root/'src/graphics/fonts/HermesX_zh/HermesX_CN12_data.cpp',lib/'OLEDDisplayFonts.cpp']+[base/(n+'.cpp') for n in ['HermesXEmergencyConfirmUiRenderer','HermesXLowMemoryUiRenderer','HermesXRotaryLockUiRenderer']]
subprocess.run(['clang++','-std=c++17','-include',str(out/'Arduino.h'),'-DHERMESX_NRF52840_PROMICRO=1','-I'+str(out),'-I'+str(root/'src'),'-I'+str(lib),*[str(p) for p in sources],'-o',str(out/'preview')],check=True)
subprocess.run([str(out/'preview')],check=True,cwd=out)
from PIL import Image,ImageDraw
files=sorted(out.glob('*.pgm'));sheet=Image.new('RGB',(128*4*3, (64*4+30)*((len(files)+2)//3)), '#20252b');draw=ImageDraw.Draw(sheet)
for i,p in enumerate(files):
 x=(i%3)*512;y=(i//3)*286
 draw.text((x+4,y+5),p.stem,fill='white');sheet.paste(Image.open(p).resize((512,256),Image.Resampling.NEAREST),(x,y+26))
sheet.save(out/'preview.png')
