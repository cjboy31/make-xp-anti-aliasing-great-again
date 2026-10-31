-- SPDX-License-Identifier: MIT
-- Run with Lua 5.1 or LuaJIT. SDK doubles keep tests outside a live simulator.
local path=assert(arg[1],"script path required")
local state,macros,exit_code
local sdk={}
function sdk.XPLMFindDataRef(name) assert(name=="sim/private/controls/fbo/shadow_cam_size");if state.missing then return nil end;return 1 end
function sdk.XPLMCanWriteDataRef() return state.readonly and 0 or 1 end
function sdk.XPLMGetDataRefTypes() return state.kind or 2 end
function sdk.XPLMGetDataf() return state.value end
sdk.XPLMGetDatai=sdk.XPLMGetDataf
function sdk.XPLMSetDataf(_,value) state.writes=state.writes+1;if not state.reject then state.value=value end end
sdk.XPLMSetDatai=sdk.XPLMSetDataf
package.preload.ffi=function() return {C=sdk,cdef=function()end,load=function()return sdk end} end
package.preload.bit=function() return {band=function(a,b) return math.floor(a/b)%2==1 and b or 0 end} end
function logMsg()end
function add_macro(label,command) macros[label]=command end
function do_on_exit(code)exit_code=code end
local function load(options)
    state=options or {};state.value=state.value or 2048;state.writes=0;macros={};exit_code=nil
    MXPS_Enhanced_Select=nil;MXPS_Enhanced_Restore=nil
    dofile(path)
end
load();assert(state.writes==0 and state.value==2048,"script must start off")
assert(exit_code and macros['Make XP shadows: 4K - recommended'],"restore and macro controls missing")
MXPS_Enhanced_Select(4096);assert(state.value==4096 and state.writes==1)
MXPS_Enhanced_Select(8192);assert(state.value==8192)
MXPS_Enhanced_Select(0);assert(state.value==2048,"Off failed to restore session baseline")
MXPS_Enhanced_Select(4096);state.value=1024;MXPS_Enhanced_Select(0)
assert(state.value==1024,"Off overwrote a different add-on")
MXPS_Enhanced_Select(4096);state.value=2048;local writes=state.writes;MXPS_Enhanced_Select(8192)
assert(state.value==2048 and state.writes==writes,"script fought another add-on")
MXPS_Enhanced_Select(4096);assert(state.value==4096);assert(loadstring(exit_code))()
assert(state.value==2048,"exit failed to restore owned setting")
load();MXPS_Enhanced_Select(999999);assert(state.writes==0,"unlisted value accepted")
load{readonly=true};assert(not MXPS_Enhanced_Select and state.writes==0,"read-only ref touched")
load{missing=true};assert(not MXPS_Enhanced_Select and state.writes==0,"missing ref touched")
load{kind=16};assert(not MXPS_Enhanced_Select and state.writes==0,"array ref touched")
load{value=0};MXPS_Enhanced_Select(4096);assert(state.writes==0,"invalid baseline touched")
load{kind=1};MXPS_Enhanced_Select(4096);MXPS_Enhanced_Select(0);assert(state.value==2048,"integer SDK access failed")
load{reject=true};MXPS_Enhanced_Select(4096);assert(state.value==2048,"rejected write changed baseline")
print('PASS: starts Off, bounded presets, float/int access, restore/exit, conflicting owner, missing/read-only/wrong-type/rejected controls')
