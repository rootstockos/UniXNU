#!/usr/bin/env python3
"""Offline A8xx HFI additions: the GPU_PERF table and the MX dependency votes against the original mainline functions.

The original a8xx_hfi_send_perf_table (a6xx_hfi.c) and a6xx_gmu_rpmh_dep_votes_init (a6xx_gmu.c) are extracted from
the pinned torvalds/linux revision (scripts/data/adreno-a8xx-inputs.json) into an oracle that stays in ignored build
storage; only its platform/types and message sink are substituted, and it performs no device I/O. kits/AdrenoFirmware
test_a8xx.c compares the kit's encoder and vote builder with it. --fetch makes the sparse checkout in .cache/linux-a8xx.
On a non-AArch64 host the kit's AdrenoHfi.c is compiled from a copy whose only change is its barriers as compiler
fences (the startup library links against the transport)."""
import hashlib, json, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
kit = ROOT / 'kits/AdrenoFirmware'
out = ROOT / 'build/adreno-hfi-a8xx'
out.mkdir(parents=True, exist_ok=True)
pin = json.loads((ROOT / 'scripts/data/adreno-a8xx-inputs.json').read_text())['linux']
cache = ROOT / '.cache/linux-a8xx'
if '--fetch' in sys.argv[1:]:
    if not (cache / '.git').exists():
        cache.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'init', '-q', str(cache)], check=True)
        subprocess.run(['git', '-C', str(cache), 'remote', 'add', 'origin', pin['repository']], check=True)
    subprocess.run(['git', '-C', str(cache), 'sparse-checkout', 'set', '--no-cone', '/drivers/gpu/drm/msm/adreno/'],
                   check=True)
    subprocess.run(['git', '-C', str(cache), 'fetch', '-q', '--depth', '1', '--filter=blob:none', 'origin',
                    pin['commit']], check=True)
    subprocess.run(['git', '-C', str(cache), 'checkout', '-q', 'FETCH_HEAD'], check=True)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for name, digest in pin['files'].items():
    if not (cache / name).exists() or sha(cache / name) != digest:
        raise SystemExit('Pinned source missing or changed: ' + name + ' (run with --fetch)')
hfi_c = (cache / 'drivers/gpu/drm/msm/adreno/a6xx_hfi.c').read_text()
hfi_h = (cache / 'drivers/gpu/drm/msm/adreno/a6xx_hfi.h').read_text()
gmu_c = (cache / 'drivers/gpu/drm/msm/adreno/a6xx_gmu.c').read_text()
def function(source, signature):
    start = source.index(signature); at = source.index('{', start) + 1; depth = 1
    while depth:
        depth += {'{': 1, '}': -1}.get(source[at], 0); at += 1
    return source[start:at]
prelude = '''#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "AdrenoHfiStartup.h"
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint64_t u64;typedef int32_t s32;
typedef int spinlock_t;typedef int atomic_t;
#define __packed __attribute__((packed))
#define __le32 uint32_t
#define __le16 uint16_t
#define DRM_DEV_ERROR(...) ((void)0)
#define GFP_KERNEL 0
#define kzalloc(size,flags) calloc(1,(size))
#define kfree free
#define ENOMEM 12
#define EINVAL 22
#define IS_ERR(p) ((p)==NULL)
#define PTR_ERR(p) (-EINVAL)
''' + hfi_h + '''
struct device {int unused;};
struct a6xx_gmu {
 int nr_gpu_freqs,nr_gmu_freqs;unsigned long gpu_freqs[16],gmu_freqs[4];
 u32 gx_arc_votes[16],dep_arc_votes[16],cx_arc_votes[4];
};
static unsigned *sink_id,*sink_words;static u32 *sink_data;
static int a6xx_hfi_send_msg(struct a6xx_gmu *g,int id,void *msg,u32 bytes,u32 *output,u32 capacity) {
 (void)g;(void)output;(void)capacity;
 if(bytes<4 || bytes>640 || bytes%4)return -1;
 *sink_id=(unsigned)id;*sink_words=bytes/4-1;memcpy(sink_data,(u32 *)msg+1,bytes-4);return 0;
}
static const u16 *oracle_mx;static size_t oracle_mx_bytes;
static const u16 *cmd_db_read_aux_data(const char *id,size_t *count) {
 if(strcmp(id,"mx.lvl"))return NULL;*count=oracle_mx_bytes;return oracle_mx;
}
static const unsigned long *oracle_freqs;static const uint32_t *oracle_levels;static unsigned oracle_count;
static unsigned int a6xx_gmu_get_arc_level(struct device *dev,unsigned long freq) {
 (void)dev;for(unsigned i=0;i<oracle_count;++i)if(oracle_freqs[i]==freq)return oracle_levels[i];return 0;
}
'''
wrapper = '''
int source_perf_table(const AdrenoHfiStartupConfig *c,unsigned *id,unsigned *words,u32 payload[159]) {
 struct a6xx_gmu g={0};g.nr_gpu_freqs=(int)c->gpu_count;g.nr_gmu_freqs=(int)c->gmu_count;
 for(unsigned i=0;i<c->gpu_count;++i){g.gpu_freqs[i]=c->gpu[i].hz;g.gx_arc_votes[i]=c->gpu[i].vote;g.dep_arc_votes[i]=c->gpu_dependency[i];}
 for(unsigned i=0;i<c->gmu_count;++i){g.gmu_freqs[i]=c->gmu[i].hz;g.cx_arc_votes[i]=c->gmu[i].vote;}
 sink_id=id;sink_words=words;sink_data=payload;return a8xx_hfi_send_perf_table(&g);
}
int source_dependency_votes(const uint32_t *levels,unsigned count,const u16 *mx,unsigned mx_count,uint32_t *votes) {
 static unsigned long freqs[16];struct device dev;
 for(unsigned i=0;i<count;++i)freqs[i]=i?1000000ul*i:0;
 oracle_freqs=freqs;oracle_levels=levels;oracle_count=count;oracle_mx=mx;oracle_mx_bytes=2u*mx_count;
 return a6xx_gmu_rpmh_dep_votes_init(&dev,votes,freqs,(int)count);
}
'''
oracle = out / 'original-a8xx-oracle.c'
oracle.write_text(prelude + function(hfi_c, 'static int a8xx_hfi_send_perf_table(') + '\n' +
                  function(gmu_c, 'static int a6xx_gmu_rpmh_dep_votes_init(') + wrapper)
transport = kit / 'AdrenoHfi.c'
if subprocess.run(['uname', '-m'], capture_output=True, text=True).stdout.strip() != 'arm64':
    text = transport.read_text()
    for old, new in [('#if !defined(__aarch64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__\n',
                      '#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__\n'),
                     ('__asm__ volatile("dmb oshld":::"memory");', '__atomic_thread_fence(__ATOMIC_ACQUIRE);'),
                     ('__asm__ volatile("dsb oshst":::"memory");', '__atomic_thread_fence(__ATOMIC_RELEASE);'),
                     ('__asm__ volatile("dmb osh":::"memory");', '__atomic_thread_fence(__ATOMIC_SEQ_CST);')]:
        if text.count(old) != 1:
            raise SystemExit('AdrenoHfi.c changed; the host copy no longer applies')
        text = text.replace(old, new)
    if '__asm__' in text:
        raise SystemExit('AdrenoHfi.c has another barrier')
    transport = out / 'host-AdrenoHfi.c'
    transport.write_text(text)
sources = [transport, kit / 'AdrenoHfiStartup.c', kit / 'AdrenoHfiVotes.c']
for name, extra in [('test-a8xx', [kit / 'test_a8xx.c', oracle, '-DSOURCE_ORACLE', '-Wno-unused-function']),
                    ('test-a8xx-plain', [kit / 'test_a8xx.c']),
                    ('test-startup', [kit / 'test_startup.c'])]:
    binary = out / name
    subprocess.run(['clang', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(kit), *map(str, sources),
                    *map(str, extra), '-o', str(binary)], check=True, timeout=120)
    result = subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=60)
    print(name + ': ' + result.stdout.strip().splitlines()[-1])
report = dict(linux_revision=pin['commit'], source_sha256=pin['files'],
              kit_sha256={p.name: sha(p) for p in [kit / 'AdrenoHfiStartup.c', kit / 'AdrenoHfiStartup.h',
                                                    kit / 'AdrenoHfiVotes.c', kit / 'AdrenoHfiVotes.h',
                                                    kit / 'test_a8xx.c']},
              oracle_in_git=False, hardware_observed=False)
(out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
