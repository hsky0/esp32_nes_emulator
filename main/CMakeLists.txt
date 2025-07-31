FILE(GLOB_RECURSE srcs  "main.c"
                        "../components/nofrendo/*.*"
                        "../components/nofrendo/cpu/*.*"
                        "../components/nofrendo/libsnss/*.*"
                        "../components/nofrendo/mappers/*.*"
                        "../components/nofrendo/sndhrdw/*.*"
                        "../components/nofrendo/nes/*.*"
                        "../components/nofrendo-esp32/*.*"
)

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "."
                    "../components/nofrendo/"
                    "../components/nofrendo/cpu/"
                    "../components/nofrendo/libsnss/"
                    "../components/nofrendo/mappers/"
                    "../components/nofrendo/sndhrdw/"
                    "../components/nofrendo/nes/"
                    "../components/nofrendo-esp32/"
                    )

