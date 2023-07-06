mkdir compiled 2>/dev/null

for i in *.vert *.tesc *.tese *.geom *.frag *.comp *.rgen *.rint *.rahit *.rchit *.rmiss *.rcall *.task *.mesh
do
	if ! [ -e "$i" ]; then continue; fi
	
	echo "try to compile $i"
	glslc -O --target-env=vulkan1.3 "$i" -o "compiled/$i.spv"
done
