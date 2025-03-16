#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <vorbis/vorbisfile.h>
#include <ogg/logger.h>

struct vorbis_data {
  const uint8_t *current;
  const uint8_t *data;
  size_t size;
};

size_t read_func(void *ptr, size_t size1, size_t size2, void *datasource) {
  vorbis_data* vd = (vorbis_data *)(datasource);
  size_t len = size1 * size2;
  log_message("[TRACE] Hash: 529f97a66abfe2eee63599f1e5c5e06e, File: vorbis/contrib/oss-fuzz/decode_fuzzer.cc, Func: _Z9read_funcPvmmS_, Line: 15, Col: 7\n");
  if (vd->current + len > vd->data + vd->size) {
      len = vd->data + vd->size - vd->current;
  }
  memcpy(ptr, vd->current, len);
  vd->current += len;
  return len;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  log_init("program.log"); 
  ov_callbacks memory_callbacks = {0};
  memory_callbacks.read_func = read_func;
  vorbis_data data_st;
  data_st.size = Size;
  data_st.current = Data;
  data_st.data = Data;
  OggVorbis_File vf;
  int result = ov_open_callbacks(&data_st, &vf, NULL, 0, memory_callbacks);
  if (result < 0) {
    return 0;
  }
  int current_section = 0;
  int eof = 0;
  char buf[4096];
  int read_result;
  log_message("[TRACE] Hash: 4e69ad006785123542cbde2f273d1e89, File: vorbis/contrib/oss-fuzz/decode_fuzzer.cc, Func: LLVMFuzzerTestOneInput, Line: 40, Col: 10\n");
  while (!eof) {
    read_result = ov_read(&vf, buf, sizeof(buf), 0, 2, 1, &current_section);
    log_message("[TRACE] Hash: 98e84fd812d10ed73778ff1c62cf2aeb, File: vorbis/contrib/oss-fuzz/decode_fuzzer.cc, Func: LLVMFuzzerTestOneInput, Line: 42, Col: 9\n");
    log_message("[TRACE] Hash: f92ee9d08c5190604d30416f513b920c, File: vorbis/contrib/oss-fuzz/decode_fuzzer.cc, Func: LLVMFuzzerTestOneInput, Line: 42, Col: 35\n");
    if (read_result != OV_HOLE && read_result <= 0) {
      eof = 1;
    }
  }
  ov_clear(&vf);
  log_close();
  return 0;
}
