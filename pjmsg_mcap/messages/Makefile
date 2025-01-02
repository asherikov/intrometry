OUTPUT_DIR=../src/3rdparty/generated

generate: clean
	mkdir -p ${OUTPUT_DIR}
	~/Fast-DDS-Gen/scripts/fastddsgen \
		-default_extensibility final \
		-flat-output-dir \
		-fusion \
		-language c++ \
		-typeros2 \
		-no-typeobjectsupport \
		-I ./idl/ \
		-d ${OUTPUT_DIR}/ \
		idl/*.idl
	rm -rf ${OUTPUT_DIR}/*PubSubTypes*

clean:
	rm -rf ${OUTPUT_DIR}
