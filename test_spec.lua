#!/usr/bin/env lua

local VISUALDELAY = os.getenv("VISUALDELAY")
 
local visual = VISUALDELAY or false
local visual_delay = VISUALDELAY and (tonumber(VISUALDELAY)) or 0.1

local unistd = require("posix.unistd")
local time = require("posix.time")
local curses = require("posix.curses")
local rote = require("rote")

local rt = rote.RoteTerm(24, 80)

local function os_execread(cmd)
   local fd = io.popen(cmd, "r")
   local out = fd:read("*a")
   fd:close()
   return (out:gsub("\n$", ""))
end
local branch = os_execread("git branch | grep '*'"):sub(3)

print("Running in branch "..branch)

os.execute("make coverage")
os.execute("rm -f *.gcda */*.gcda")
os.execute("rm -f coverage.info test.htoprc")
os.execute("rm -rf lcov")
os.execute("killall htop")
os.execute("ps aux | grep '[s]leep 12345' | awk '{print $2}' | xargs kill 2> /dev/null")

os.execute("cp ./default.htoprc ./test.htoprc")
rt:forkPty("LC_ALL=C HTOPRC=./test.htoprc ./htop")

local stdscr, term_win
-- Curses initalization needed even when not in visual mode
-- because luaposix only initializes KEY_* constants after initscr().
stdscr = curses.initscr()
if visual then
   curses.echo(false)
   curses.start_color()
   curses.raw(true)
   curses.halfdelay(1)
   stdscr:keypad(true)
   term_win = curses.newwin(24, 80, 0, 0)
   local function makePair(foreground, background)
      return background * 8 + 7 - foreground
   end
   -- initialize the color pairs the way rt:draw() expects it
   for foreground = 0, 7 do
      for background = 0, 7 do
         if foreground ~= 7 or background ~= 0 then
            local pair = makePair(foreground, background)
            curses.init_pair(pair, foreground, background)
         end
      end
   end
else
   curses.endwin()
end

local function delay(t)
   time.nanosleep({ tv_sec = math.floor(t), tv_nsec = (t - math.floor(t)) * 1000000000 })
end

local function show(key)
   rt:update()
   if visual then
      rt:draw(term_win, 0, 0)
      if key then
         term_win:mvaddstr(0, 0, tostring(key))
      end
      term_win:refresh()
      
      delay(visual_delay)
   end
end

local function send(key, times, quick)
   if times == 0 then return end
   for _ = 1, times or 1 do
      delay(0.003) -- 30ms delay to avoid clobbering Esc sequences
      if type(key) == "string" then
         for c in key:gmatch('.') do
            rt:keyPress(string.byte(c))
         end
      else
         rt:keyPress(key)
      end
      if not quick then
         show(key)
      end
   end
   if quick then
      show(key)
   end
end

local function string_at(x, y, len)
   rt:update()
   local out = {}
   for i = 1, len do
      out[#out+1] = rt:cellChar(y-1, x+i-2)
   end
   return table.concat(out)
end

local function is_string_at(x, y, str)
   return string_at(x, y, #str) == str
end

local function check_string_at(x, y, str)
   return { str, string_at(x, y, #str) }
end

local ESC = "\27\27"

time.nanosleep({ tv_sec = 0, tv_nsec = 150000000 }) -- give some time for htop to initialize.

local y_panelhdr = (function()
   for y = 1, 24 do
      if is_string_at(3, y, "PID") then
         return y
      end
   end
end)() or 1

local x_metercol2 = (branch == "wip") and 41 or 43

show()

os.execute("sleep 12345 &")

local function terminated()
   return not os.execute("ps aux | grep -q '\\./[h]top'")
end

local function running_it(desc, fn)
   it(desc, function()
      assert(not terminated())
      show()
      fn()
      assert(not terminated())
   end)
end

local function check(t)
   return t[1], t[2]
end

local attrs = {
   black_on_cyan = 6,
   red_on_cyan = 22,
   white_on_black = 176,
   yellow_on_black = 112,
}

local function find_selected_y(from)
   rt:update()
   for y = from or (y_panelhdr + 1), rt:rows() - 1 do
      local attr = rt:cellAttr(y-1, 1)
      if attr == attrs.black_on_cyan then
         return y
      end
   end
   return y_panelhdr + 1
end

local function find_command_x()
   for x = 1, 80 do
      if is_string_at(x, y_panelhdr, "Command") then
         return x
      end
   end
   return 64
end

local function set_display_option(n)
   send("S")
   send(curses.KEY_DOWN)
   send(curses.KEY_RIGHT)
   send(curses.KEY_DOWN, n)
   send("\n")
   send(curses.KEY_F10)
end

describe("htop test suite", function()
   
   running_it("performs incremental filter", function()
      send("\\")
      send("x\127bux\127sted") -- test backspace
      send("\n")
      delay(0.2)
      rt:update()
      local pid = ("      "..tostring(unistd.getpid())):sub(-5)
      local ourpid = check_string_at(1, y_panelhdr + 1, pid)
      send("\\")
      send(ESC)
      send(curses.KEY_F5)
      send(curses.KEY_HOME)
      delay(0.15)
      rt:update()
      local initpid = check_string_at(1, y_panelhdr + 1, "    1")
      delay(0.15)
      rt:update()
      send(curses.KEY_F5)
      assert.equal(check(ourpid))
      assert.equal(check(initpid))
   end)

   running_it("performs incremental search", function()
      send(curses.KEY_HOME)
      send("/")
      send("busted")
      local attr = rt:cellAttr(rt:rows() - 1, 30)
      send("\n")
      delay(0.4)
      local line = find_selected_y()
      local pid = ("      "..tostring(unistd.getpid())):sub(-5)
      assert.equal(attr, attrs.black_on_cyan)
      local ourpid = check_string_at(1, line, pid)
      send(curses.KEY_HOME)
      assert.equal(check(ourpid))
   end)

   running_it("kills a process", function()
      send(curses.KEY_HOME)
      send("\\")
      send("sleep 12345")
      local attr = rt:cellAttr(rt:rows() - 1, 30)
      assert.equal(attr, attrs.black_on_cyan)
      send("\n")
      delay(0.3)
      rt:update()
      local col = find_command_x()
      local procname = check_string_at(col, y_panelhdr + 1, "sleep 12345")
      send("k")
      send("\n")
      send("\\")
      send(ESC)
      delay(0.3)
      assert.equal(check(procname))
      assert.not_equal((os.execute("ps aux | grep -q '[s]leep 12345'")), true)
   end)

   running_it("runs strace", function()
      send(curses.KEY_HOME)
      send("/")
      send("busted")
      send("\n")
      send("s")
      delay(1)
      send(ESC)
   end)

   running_it("runs lsof", function()
      send(curses.KEY_HOME)
      send("/")
      send("busted")
      send("\n")
      send("l")
      delay(1)
      send(ESC)
   end)

   running_it("performs filtering in lsof", function()
      send(curses.KEY_HOME)
      send("/")
      send("htop")
      send("\n")
      send("l")
      send(curses.KEY_F4)
      send("pipe")
      delay(1)
      local pipefd = check_string_at(1, 3, "    3")
      send(ESC)
      assert.equal(check(pipefd))
   end)

   running_it("performs search in lsof", function()
      send(curses.KEY_HOME)
      send("/")
      send("htop")
      send("\n")
      send("l")
      send(curses.KEY_F3)
      send("pipe")
      delay(1)
      local line = find_selected_y(3)
      local pipefd = check_string_at(1, line, "    3")
      send(ESC)
      assert.equal(check(pipefd))
   end)


   running_it("cycles through meter modes in the default meters", function()
      send("S")
      for _ = 1, 2 do
         send(curses.KEY_RIGHT)
         for _ = 1, 3 do
            send("\n", 4)
            send(curses.KEY_DOWN)
         end
      end
      send(ESC)
   end)

   running_it("show process of a user", function()
      send(curses.KEY_F5)
      send("u")
      send(curses.KEY_DOWN)
      delay(0.3)
      rt:update()
      local chosen = string_at(1, y_panelhdr + 2, 9)
      send("\n")
      send(curses.KEY_HOME)
      delay(0.3)
      rt:update()
      local shown = string_at(7, y_panelhdr + 1, 9)
      send("u")
      send("\n")
      send(curses.KEY_HOME)
      delay(0.3)
      rt:update()
      local inituser = string_at(7, y_panelhdr + 1, 9)
      send(curses.KEY_F5)
      assert.equal(shown, chosen)
      assert.equal(inituser, "root     ")
   end)

   running_it("performs failing search", function()
      send(curses.KEY_HOME)
      send("/")
      send("xxxxxxxxxx")
      delay(0.3)
      rt:update()
      local attr = rt:cellAttr(rt:rows() - 1, 30)
      assert.equal(attr, attrs.red_on_cyan)
      send("\n")
   end)

   running_it("cycles through search", function()
      send(curses.KEY_HOME)
      send("/")
      send("sh")
      local lastpid
      local pidpairs = {}
      for _ = 1, 3 do
         send(curses.KEY_F3)
         local line = find_selected_y()
         local pid = string_at(1, line, 5)
         if lastpid then
            pidpairs[#pidpairs + 1] = { lastpid, pid }
            lastpid = pid
         end
      end
      send(curses.KEY_HOME)
      for _, pair in pairs(pidpairs) do
         assert.not_equal(pair[1], pair[2])
      end
   end)
   
   running_it("visits each setup screen", function()
      send("S")
      send(curses.KEY_DOWN, 3)
      send(curses.KEY_F10)
   end)
   
   running_it("adds and removes PPID column", function()
      send("S")
      send(curses.KEY_DOWN, 3)
      send(curses.KEY_RIGHT, 2)
      send(curses.KEY_DOWN, 2)
      send("\n")
      send(curses.KEY_F10)
      delay(0.2)
      local ppid = check_string_at(2, y_panelhdr, "PPID")
      send("S")
      send(curses.KEY_DOWN, 3)
      send(curses.KEY_RIGHT, 1)
      send(curses.KEY_DC)
      send(curses.KEY_F10)
      delay(0.2)
      local not_ppid = check_string_at(2, y_panelhdr, "PPID")
      assert.equal(check(ppid))
      assert.not_equal(check(not_ppid))
   end)
   
   running_it("changes CPU affinity for a process", function()
      send("a")
      send(" \n")
      send(ESC)
   end)

   running_it("renices for a process", function()
      send("/")
      send("busted")
      send("\n")
      local line = find_selected_y()
      local before = check_string_at(22, line, " 0")
      send(curses.KEY_F8)
      delay(0.3)
      local after = check_string_at(22, line, " 1")
      assert.equal(check(before))
      assert.equal(check(after))
   end)
   
   running_it("changes IO priority for a process", function()
      send("/")
      send("htop")
      send("\n")
      send("i")
      send(curses.KEY_END)
      send("\n")
      send(ESC)
   end)

   running_it("shows help", function()
      send(curses.KEY_F1)
      send("\n")
      set_display_option(9)
      send(curses.KEY_F1)
      send("\n")
      set_display_option(9)
   end)
   
   local meters = {
      { name = "clock", down = 0, string = "Time" },
      { name = "load", down = 2, string = "Load" },
      { name = "battery", down = 7, string = "Battery" },
      { name = "hostname", down = 8, string = "Hostname" },
   }
   
   for _, item in ipairs(meters) do
      running_it("adds and removes a "..item.name.." widget", function()
         send("S")
         send(curses.KEY_RIGHT, 3)
         send(curses.KEY_DOWN, item.down)
         if branch == "wip" then
            send("\n")
            send(curses.KEY_UP, 4)
            send("\n")
         else
            send(curses.KEY_F6)
            send(curses.KEY_LEFT)
            send(curses.KEY_DOWN, 4)
            send(curses.KEY_F7, 4)
         end
         send(curses.KEY_F4, 4) -- cycle through meter modes
         delay(0.15)
         rt:update()
         local with = check_string_at(x_metercol2, 2, item.string)
         send(curses.KEY_DC)
         delay(0.15)
         local without = check_string_at(x_metercol2, 2, item.string)
         send(curses.KEY_F10)
         assert.equal(check(with))
         assert.not_equal(check(without))
      end)
   end

   running_it("goes through themes", function()
      send(curses.KEY_F2)
      send(curses.KEY_DOWN, 2)
      send(curses.KEY_RIGHT)
      for _ = 1, 6 do
         send("\n")
         send(curses.KEY_DOWN)
      end
      send(curses.KEY_UP, 6)
      send("\n")
      send(curses.KEY_F10)
   end)
   
   local display_options = {
      { name = "tree view", down = 0 },
      { name = "shadow other user's process", down = 1 },
      { name = "hide kernel threads", down = 2 },
      { name = "hide userland threads", down = 3 },
      { name = "display threads in different color", down = 4 },
      { name = "show custom thread names", down = 5 },
      { name = "highlight basename", down = 6 },
      { name = "highlight large numbers", down = 7 },
      { name = "leave margin around header", down = 8 },
      { name = "use detailed CPU time", down = 9 },
      { name = "count from zero", down = 10 },
      { name = "update process names", down = 11 },
      { name = "guest time in CPU%", down = 12 },
   }
   
   for _, item in ipairs(display_options) do
      running_it("checks display option to "..item.name, function()
         for _ = 1, 2 do
            set_display_option(item.down)
            delay(0.1)
         end
      end)
   end

   running_it("shows detailed CPU with guest time", function()
      for _ = 1, 2 do
         send("S")
         send(curses.KEY_DOWN)
         send(curses.KEY_RIGHT)
         send(curses.KEY_DOWN, 9)
         send("\n")
         send(curses.KEY_DOWN, 3)
         send("\n")
         send(curses.KEY_F10)
         delay(0.1)
      end
   end)

   running_it("expands and collapses tree", function()
      send(curses.KEY_F5) -- tree view
      send(curses.KEY_HOME)
      send(curses.KEY_DOWN) -- second process in the tree
      send("-")
      send("+")
      send(curses.KEY_F5)
   end)

   running_it("sets sort key", function()
      send(".")
      send("\n")
   end)

   running_it("tags all children", function()
      send(curses.KEY_F5) -- tree view
      send(curses.KEY_HOME) -- ensure we're at init
      send("c")
      local taggedattrs = {}
      rt:update()
      for y = y_panelhdr + 2, 23 do
         table.insert(taggedattrs, rt:cellAttr(y-1, 4))
      end
      delay(0.2)
      send("U")
      local untaggedattrs = {}
      rt:update()
      for y = y_panelhdr + 2, 23 do
         table.insert(untaggedattrs, rt:cellAttr(y-1, 4))
      end
      send(curses.KEY_F5)

      for _, taggedattr in ipairs(taggedattrs) do
         assert.equal(attrs.yellow_on_black, taggedattr)
      end
      for _, untaggedattr in ipairs(untaggedattrs) do
         assert.equal(attrs.white_on_black, untaggedattr)
      end
   end)
   
   for i = 1, 62 do
      running_it("show column "..i, function()
         send("S")
         send(curses.KEY_END)
         send(curses.KEY_RIGHT, 1)
         if i > 1 then
            send(curses.KEY_DC)
         end
         send(curses.KEY_RIGHT, 1)
         local down = i
         while down > 13 do
            send(curses.KEY_NPAGE)
            down = down - 13
         end
         send(curses.KEY_DOWN, down, "quick")
         send("\n")
         send(curses.KEY_F10)
         if i == 62 then
            send("S")
            send(curses.KEY_END)
            send(curses.KEY_RIGHT, 1)
            if i > 1 then
               send(curses.KEY_DC)
            end
            send(curses.KEY_F10)
         end
      end)
   end
   
   it("finally quits", function()
      assert(not terminated())
      send("q")
      while not terminated() do
         unistd.sleep(1)
      end
      assert(terminated())
      if visual then
         curses.endwin()
      end
      os.execute("make lcov && xdg-open lcov/index.html")
   end)
end)

