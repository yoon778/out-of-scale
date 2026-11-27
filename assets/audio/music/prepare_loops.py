"""Finalize original full-file loops: mono32k PCM16, soft -20LUFS, short seam correction.
Usage: python prepare_loops.py --renders <studio-family-folder> --output <folder>
Pass --ffmpeg if FFmpeg is installed outside PATH.
All synthesizer notes live in the four .score.json files; no remote generation.
"""
import argparse
import concurrent.futures
import hashlib
import json
import math
import os
import subprocess
import wave
from pathlib import Path

import numpy as np

NAMES = ('school', 'military', 'office', 'home')


def run(room, renders, output, ffmpeg):
 source=renders/room/'final-r2'/'loop.wav'
 scratch=renders/room/'mono32-r2.wav'
 cmd=[ffmpeg,'-hide_banner','-nostdin','-y','-i',str(source),'-map_metadata','-1','-ac','1','-af','aformat=channel_layouts=mono,loudnorm=I=-20:TP=-3:LRA=11:linear=false','-ar','32000','-c:a','pcm_s16le',str(scratch)]
 subprocess.run(cmd,check=True,capture_output=True)
 with wave.open(str(scratch),'rb') as w:
  rate,channels,bits,frames=w.getframerate(),w.getnchannels(),w.getsampwidth()*8,w.getnframes()
  samples=np.frombuffer(w.readframes(frames),dtype='<i2').astype(np.float64)/32768
 assert (rate,channels,bits,frames)==(32000,1,16,1536000),(room,rate,channels,bits,frames)
 # Join both ends at their average without a fade or an inserted silent gap.
 # A 16ms cubic correction has zero slope at its outer edges and touches <.07% of a loop.
 raw_step=float(samples[0]-samples[-1]); n=int(rate*.016)
 t=np.linspace(0,1,n);smooth=t*t*(3-2*t)
 samples[:n]-=raw_step*.5*(1-smooth)
 samples[-n:]+=raw_step*.5*smooth
 if np.max(np.abs(samples))>=1:raise RuntimeError('Unexpected clipping after seam repair')
 pcm=np.rint(samples*32768).astype('<i2')
 assert pcm[0]==pcm[-1]
 final=output/(room+'.wav')
 with wave.open(str(final),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(pcm.tobytes())
 actual=pcm.astype(np.float64)/32768
 def db(v):return round(20*math.log10(max(float(v),1e-12)),3)
 data=final.read_bytes();assert data[:4]==b'RIFF' and data[8:12]==b'WAVE'
 rms=float(np.sqrt(np.mean(actual**2)));peak=float(np.max(np.abs(actual)))
 assert rms>.003 and peak<.92
 edge=np.abs(np.diff(actual[-3200:]))
 stats={'file':str(final),'sample_rate':rate,'channels':1,'bits':16,'duration':frames/rate,'frames':frames,'bytes':len(data),'rms_dbfs':db(rms),'peak_dbfs':db(peak),'dc':float(np.mean(actual)),'clipped_samples':int(np.sum(np.abs(pcm.astype(np.int32))>=32767)),'seam_step_lsb':int(pcm[0])-int(pcm[-1]),'raw_join_dbfs':db(abs(raw_step)),'near_join_p999_step':float(np.quantile(edge,.999)),'sha256':hashlib.sha256(data).hexdigest()}
 # Measure the final encoded waveform; keep the report outside the repo.
 meter=subprocess.run([ffmpeg,'-hide_banner','-nostdin','-i',str(final),'-af','loudnorm=I=-20:TP=-3:LRA=11:print_format=json','-f','null',os.devnull],capture_output=True,text=True,check=True)
 log=meter.stderr; start=log.rfind('{'); measured,_=json.JSONDecoder().raw_decode(log[start:])
 stats['integrated_lufs']=float(measured['input_i']);stats['true_peak_dbtp']=float(measured['input_tp'])
 return stats


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--renders', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(
            lambda room: run(room, args.renders, args.output, args.ffmpeg), NAMES))
    assert len({result['sha256'] for result in results}) == len(NAMES)
    report = json.dumps(results, ensure_ascii=False, indent=2)
    (args.renders / 'final-wave-proof.json').write_text(report, encoding='utf-8')
    print(report)
