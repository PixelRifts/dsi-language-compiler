local timefile = io.open("temp.txt","w+");

local test_count = 4;
local tracked_frame_count = 1000
local super_frame0 = 0.0
local super_average = 0.0


for k=1,test_count do
  local average = 0.0
  local frame0  = 0.0
  local i = 0
  emu.reset()
  
  while true do
    local keys = input.get()
  
    local x = os.clock()
    emu.frameadvance()
  
    if i == 0 then
      frame0 = os.clock() - x
      timefile:write("Frame 0: "..frame0.."\n")
    elseif i <= 1001 then
      average = average + ((os.clock() - x) / 1000.0)
    elseif i == 1002 then
      timefile:write("1k frames: "..average.."\n")
      break
    end
    i = i + 1
  end
  
  print("Finished Test "..k.."\n")
  super_average = super_average + (average / test_count)
  super_frame0  = super_frame0  + (frame0  / test_count)
end

timefile:write("\n\nEnded Tests:\n")
timefile:write("Frame 0 Average: "..super_frame0.."\n");
timefile:write("Frametime Average: "..super_average.."\n");

timefile:close()
print("Finished Tests")