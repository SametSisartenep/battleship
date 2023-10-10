/*
 * standalone sound mixer based on rxi's cmixer.
 */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include "mixer.h"

static Mixer mixer;


static int
min(int a, int b)
{
	return a < b? a: b;
}

static int
max(int a, int b)
{
	return a > b? a: b;
}

static int
clamp(int n, int min, int max)
{
	return n < min? min: n > max? max: n;
}

static double
fclamp(double n, double min, double max)
{
	return n < min? min: n > max? max: n;
}

static int
float2fixed(double n)
{
	return n*FIXED_UNIT;
}

static int
fixedlerp(int a, int b, int t)
{
	return a + ((b - a)*t >> FIXED_BITS);
}

//static void
//fprintsource(int fd, AudioSource *src)
//{
//	fprint(fd, "src 0x%p:\n"
//		"udata\t0x%p\n"
//		"samplerate\t%d\n"
//		"length\t%d\n"
//		"end\t%d\n"
//		"state\t%d\n"
//		"position\t%lld\n"
//		"gain l/r\t%d/%d\n"
//		"rate\t%d\n"
//		"nextfill\t%d\n"
//		"loop\t%d\n"
//		"rewind\t%d\n"
//		"active\t%d\n"
//		"gain\t%g\n"
//		"pan\t%g\n",
//		src, src->udata, src->samplerate, src->length, src->end, src->state, src->position,
//		src->lgain, src->rgain, src->rate, src->nextfill, src->loop, src->rewind,
//		src->active, src->gain, src->pan);
//
//}

static void
pcm_handler(AudioEvent *e)
{
	Pcm *pcm;
	s16int *dst;
	int len, i, n;

	pcm = e->udata;

	switch(e->type){
	case AUDIO_EVENT_DESTROY:
		free(pcm->data);
		free(pcm);
		break;
	case AUDIO_EVENT_SAMPLES:
		dst = e->buffer;
		len = e->length/2;
Fillbuf:
		n = min(len, pcm->len - pcm->off);
		len -= n;
		while(n--){
			i = 2*pcm->off;
			dst[0] = ((s16int*)pcm->data)[i];
			dst[1] = ((s16int*)pcm->data)[i+1];
			dst += 2;
			pcm->off++;
		}
		if(len > 0){
			pcm->off = 0;
			goto Fillbuf;
		}
		break;
	case AUDIO_EVENT_REWIND:
		pcm->off = 0;
		break;
	}
}

/* TODO generalize the *decproc procedures */
static void
wavdecproc(void *arg)
{
	int *pfd, fd;

	pfd = arg;
	fd = pfd[2];

	close(pfd[0]);
	dup(fd, 0);
	close(fd);
	dup(pfd[1], 1);
	close(pfd[1]);

	execl("/bin/audio/wavdec", "wavdec", nil);
	threadexitsall("execl: %r");
}

static void
mp3decproc(void *arg)
{
	int *pfd, fd;

	pfd = arg;
	fd = pfd[2];

	close(pfd[0]);
	dup(fd, 0);
	close(fd);
	dup(pfd[1], 1);
	close(pfd[1]);

	execl("/bin/audio/mp3dec", "mp3dec", nil);
	threadexitsall("execl: %r");
}

static int
loadaudio(AudioSourceInfo *info, int fd, void (*decfn)(void*))
{
	Pcm *pcm;
	void *data;
	uchar buf[1024];
	int pfd[3], n, len;

	data = nil;
	len = 0;

	if(pipe(pfd) < 0){
		werrstr("pipe: %r");
		return -1;
	}
	pfd[2] = fd;

	procrfork(decfn, pfd, mainstacksize, RFFDG|RFNAMEG|RFNOTEG);
	close(pfd[1]);
	while((n = read(pfd[0], buf, sizeof buf)) > 0){
		data = realloc(data, len+n);
		if(data == nil){
			werrstr("realloc: %r");
			return -1;
		}
		memmove((uchar*)data+len, buf, n);
		len += n;
	}
	close(pfd[0]);

	pcm = malloc(sizeof *pcm);
	if(pcm == nil){
		free(data);
		werrstr("malloc: %r");
		return -1;
	}
	pcm->depth = 16;
	pcm->chans = 2;
	pcm->rate = 44100;
	pcm->data = data;
	pcm->len = len/(pcm->depth/8)/pcm->chans;

	info->udata = pcm;
	info->handler = pcm_handler;
	info->samplerate = pcm->rate;
	info->length = pcm->len;

//	fprint(2, "pcm 0x%p:\ndata 0x%p\nlen %d\ndepth %d\nchans %d\nrate %d\n",
//		pcm, pcm->data, pcm->len, pcm->depth, pcm->chans, pcm->rate);
//	fprint(2, "info 0x%p:\nudata 0x%p\nhandler 0x%p\nsamplerate %d\nlength %d\n",
//		info, info->udata, info->handler, info->samplerate, info->length);

	return 0;
}

void
initaudio(int samplerate)
{
	mixer.samplerate = samplerate;
	mixer.sources = nil;
	mixer.gain = FIXED_UNIT;
}

void
audio_set_master_gain(double gain)
{
	mixer.gain = float2fixed(gain);
}

static void
rewind_source(AudioSource *src)
{
	AudioEvent e;

	e.type = AUDIO_EVENT_REWIND;
	e.udata = src->udata;
	src->handler(&e);
	src->position = 0;
	src->rewind = 0;
	src->end = src->length;
	src->nextfill = 0;
}

static void
fill_source_buffer(AudioSource *src, int offset, int length)
{
	AudioEvent e;

	e.type = AUDIO_EVENT_SAMPLES;
	e.udata = src->udata;
	e.buffer = src->buffer + offset;
	e.length = length;
	src->handler(&e);
}

static void
process_source(AudioSource *src, int len)
{
	int i, n, a, b, p;
	int frame, count;
	s32int *dst;

	dst = mixer.buffer;

	/* Do rewind if flag is set */
	if(src->rewind)
		rewind_source(src);

	/* Process audio */
	while(len > 0){
		/* Get current position frame */
		frame = src->position >> FIXED_BITS;

		/* Fill buffer if required */
		if(frame + 3 >= src->nextfill){
			fill_source_buffer(src, 2*src->nextfill & MIXBUFMASK, MIXBUFSIZE/2);
			src->nextfill += MIXBUFSIZE/4;
		}

		/* Handle reaching the end of the playthrough */
		if(frame >= src->end){
			/*
			 * As streams continously fill the raw buffer in a loop we simply
			 * increment the end idx by one length and continue reading from it for
			 * another play-through
			 */
			src->end = frame + src->length;
			/* Set state and stop processing if we're not set to loop */
			if(!src->loop){
				src->state = AUDIO_STATE_STOPPED;
				break;
			}
		}

		/* Work out how many frames we should process in the loop */
		n = min(src->nextfill - 2, src->end) - frame;
		count = (n << FIXED_BITS) / src->rate;
		count = max(count, 1);
		count = min(count, len/2);
		len -= count * 2;

		/* Add audio to master buffer */
		if(src->rate == FIXED_UNIT){
			/* Add audio to buffer -- basic */
			n = 2*frame;
			for(i = 0; i < count; i++){
				dst[0] += (src->buffer[n&MIXBUFMASK] * src->lgain) >> FIXED_BITS;
				dst[1] += (src->buffer[(n+1)&MIXBUFMASK] * src->rgain) >> FIXED_BITS;
				n += 2;
				dst += 2;
			}
			src->position += count * FIXED_UNIT;
		}else{
			/* Add audio to buffer -- interpolated */
			for(i = 0; i < count; i++){
				n = (src->position >> FIXED_BITS) * 2;
				p = src->position & FIXED_MASK;

				a = src->buffer[n & MIXBUFMASK];
				b = src->buffer[(n+2) & MIXBUFMASK];
				dst[0] += fixedlerp(a, b, p)*src->lgain >> FIXED_BITS;
				n++;
				a = src->buffer[n & MIXBUFMASK];
				b = src->buffer[(n+2) & MIXBUFMASK];
				dst[1] += fixedlerp(a, b, p)*src->rgain >> FIXED_BITS;

				src->position += src->rate;
				dst += 2;
			}
		}
	}
}

void
processaudio(s16int *dst, int len)
{
	int i, x;
	AudioSource **s;

	/* Process in chunks of MIXBUFSIZE if `len` is larger than MIXBUFSIZE */
	while(len > MIXBUFSIZE){
		processaudio(dst, MIXBUFSIZE);
		dst += MIXBUFSIZE;
		len -= MIXBUFSIZE;
	}

	/* Zeroset internal buffer */
	memset(mixer.buffer, 0, len * sizeof(mixer.buffer[0]));

	/* Process active sources */
	s = &mixer.sources;
	while(*s){
		process_source(*s, len);
		/* Remove source from list if it is no longer playing */
		if((*s)->state != AUDIO_STATE_PLAYING){
			(*s)->active = 0;
			*s = (*s)->next;
		}else
			s = &(*s)->next;
	}

	/* Copy internal buffer to destination and clip */
	for(i = 0; i < len; i++){
		x = (mixer.buffer[i] * mixer.gain) >> FIXED_BITS;
		dst[i] = clamp(x, -32768, 32767);
	}
}

AudioSource *
newaudiosource(AudioSourceInfo *info)
{
	AudioSource *src;

	src = malloc(sizeof *src);
	if(src == nil){
		werrstr("allocation failed");
		return nil;
	}

	memset(src, 0, sizeof *src);
	src->handler = info->handler;
	src->length = info->length;
	src->samplerate = info->samplerate;
	src->udata = info->udata;
	audio_set_gain(src, 1);
	audio_set_pan(src, 0);
	audio_set_pitch(src, 1);
	audio_set_loop(src, 0);
	stopaudio(src);
	return src;
}

AudioSource *
loadaudiosource(char *path)
{
	AudioSourceInfo info;
	uchar buf[12];
	int fd;

	fd = open(path, OREAD);
	if(fd < 0)
		return nil;

	memset(buf, 0, sizeof buf);
	readn(fd, buf, sizeof buf);
	seek(fd, 0, 0);
	if(memcmp(buf, "ID3", 3) == 0 || (buf[0] == 0xFF && buf[1] == 0xFB)){
		if(loadaudio(&info, fd, mp3decproc) < 0){
			close(fd);
			return nil;
		}
	}else if(memcmp(buf+8, "WAVE", 4) == 0){
		if(loadaudio(&info, fd, wavdecproc) < 0){
			close(fd);
			return nil;
		}
	}else{
		werrstr("unsupported file format");
		close(fd);
		return nil;
	}
	close(fd);

	return newaudiosource(&info);
}

void
delaudiosource(AudioSource *src)
{
	AudioSource **s;
	AudioEvent e;

	if(src->active){
		s = &mixer.sources;
		while(*s) /* TODO potential spinlock. no bueno */
			if(*s == src){
				*s = src->next;
				break;
			}
	}

	e.type = AUDIO_EVENT_DESTROY;
	e.udata = src->udata;
	src->handler(&e);
	free(src);
}

double
audio_get_length(AudioSource *src)
{
	return src->length / (double)src->samplerate;
}

double
audio_get_position(AudioSource *src)
{
	return ((src->position >> FIXED_BITS) % src->length) / (double)src->samplerate;
}

int
audio_get_state(AudioSource *src)
{
	return src->state;
}

static void
recalc_source_gains(AudioSource *src)
{
	double l, r;
	double pan;

	pan = src->pan;
	l = src->gain * (pan <= 0 ? 1 : 1.0 - pan);
	r = src->gain * (pan >= 0 ? 1 : 1.0 + pan);
	src->lgain = float2fixed(l);
	src->rgain = float2fixed(r);
}

void
audio_set_gain(AudioSource *src, double gain)
{
	src->gain = gain;
	recalc_source_gains(src);
}

void
audio_set_pan(AudioSource *src, double pan)
{
	src->pan = fclamp(pan, -1.0, 1.0);
	recalc_source_gains(src);
}

void
audio_set_pitch(AudioSource *src, double pitch)
{
	double rate;

	if(pitch > 0)
		rate = (double)src->samplerate / mixer.samplerate * pitch;
	else
		rate = 0.001;
	src->rate = float2fixed(rate);
}

void
audio_set_loop(AudioSource *src, int loop)
{
	src->loop = loop;
}

void
playaudio(AudioSource *src)
{
	src->state = AUDIO_STATE_PLAYING;
	if(!src->active){
		src->active = 1;
		src->next = mixer.sources;
		mixer.sources = src;
	}
}

void
pauseaudio(AudioSource *src)
{
	src->state = AUDIO_STATE_PAUSED;
}

void
stopaudio(AudioSource *src)
{
	src->state = AUDIO_STATE_STOPPED;
	src->rewind = 1;
}
