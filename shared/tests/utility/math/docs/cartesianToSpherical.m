function spherConversion = cartesianToSpherical(inputVec)
%This function converts cartesian coordinate vectors to spherical coordinate vectors
x = double(inputVec(1));
y = double(inputVec(2));
z = double(inputVec(3));

r = sqrt((x * x) + (y * y) + (z * z));
theta = atan2(y, x);

if x == 0
    phi = 0;
else
    phi = asin(z / r);
end

spherConversion = [r theta phi];
end
