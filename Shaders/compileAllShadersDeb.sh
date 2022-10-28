for i in `ls *.vert *.tesc *.tese *.geom *.frag *.comp *.rgen *.rint *.rahit *.rchit *.rmiss *.rcall *.task *.mesh 2>/dev/null`

do
	glslangValidator -g --target-env vulkan1.2 $i -o $i.spv
done
