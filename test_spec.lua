#!/usr/bin/env lua

local VISUALDELAY = os.getenv("VISUALDELAY")
 
local visual = VISUALDELAY or false
local visual_delay = VISUALDELAY and (tonumber(VISUALDELAY)) or 0.1

local signal = require("posix.signal")
local unistd = require("posix.unistd")
local time = require("posix.time")
local curses = require("posix.curses")
local rote = require("rote")

local rt = rote.RoteTerm(24, 80)

os.execute("make coverage")
os.execute("rm -f *.gcda */*.gcda")
os.execute("rm -f coverage.info test.htoprc")
os.execute("rm -rf lcov")

rt:forkPty("HTOPRC=./test.htoprc ./htop")

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

local function send(key, times)
   for i = 1, times or 1 do 
      delay(0.003) -- 30ms delay to avoid clobbering Esc sequences
      if type(key) == "string" then
         for c in key:gmatch('.') do
            rt:keyPress(string.byte(c))
         end
      else
         rt:keyPress(key)
      end
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

local ESC = 27

time.nanosleep({ tv_sec = 0, tv_nsec = 150000000 }) -- give some time for htop to initialize.

local pos_panelhdr = (function()
   for i = 1, 24 do
      if is_string_at(3, i, "PID") then
         return i
      end
   end
end)() or 1

show()

local terminated = false
signal.signal(signal.SIGCHLD, function(_)
   terminated = true
end)

local function running_it(desc, fn)
   it(desc, function()
      assert(not terminated)
      show()
      fn()
      assert(not terminated)
   end)
end

local function check(t)
   return t[1], t[2]
end

describe("htop test suite", function()
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
      local ppid = check_string_at(2, pos_panelhdr, "PPID")
      send("S")
      send(curses.KEY_DOWN, 3)
      send(curses.KEY_RIGHT, 1)
      send(curses.KEY_DC)
      send(curses.KEY_F10)
      delay(0.2)
      local not_ppid = check_string_at(2, pos_panelhdr, "PPID")
      assert.equal(check(ppid))
      assert.not_equal(check(not_ppid))
   end)
   running_it("changes CPU affinity for a process", function()
      send("a")
      send(" \n")
      send(ESC)
   end)
   running_it("adds and removes a clock widget", function()
      send("S")
      send(curses.KEY_RIGHT, 3)
      send("\n")
      send(curses.KEY_UP, 4)
      send("\n")
      local time = check_string_at(41, 2, "Time")
      send(curses.KEY_DC)
      delay(0.3)
      local not_time = check_string_at(41, 2, "Time")
      send(ESC)
      assert.equal(check(time))
      assert.not_equal(check(not_time))
   end)
   running_it("adds a hostname widget", function()
      send("S")
      send(curses.KEY_RIGHT, 3)
      send(curses.KEY_DOWN, 8)
      send("\n")
      send("\n")
      send(ESC)
   end)
   it("finally quits", function()
      assert(not terminated)
      send("q")
      while not terminated do
         unistd.sleep(1)
      end
      assert(terminated)
      if visual then
         curses.endwin()
      end
      os.execute("make lcov && xdg-open lcov/index.html")
   end)
end)

