local math = math

local MaxSpeed = {}

function MaxSpeed.limit(way,max,maxf,maxb)
  if maxf and maxf>0 then
    way.city_forward_speed = math.min(way.city_forward_speed, maxf)
    way.country_forward_speed = math.min(way.country_forward_speed, maxf)
  elseif max and max>0 then
    way.city_forward_speed = math.min(way.city_forward_speed, max)
    way.country_forward_speed = math.min(way.country_forward_speed, max)
  end

  if maxb and maxb>0 then
    way.city_backward_speed = math.min(way.city_backward_speed, maxb)
    way.country_backward_speed = math.min(way.country_backward_speed, maxb)
  elseif max and max>0 then
    way.city_backward_speed = math.min(way.city_backward_speed, max)
    way.country_backward_speed = math.min(way.country_backward_speed, max)
  end
end

return MaxSpeed
