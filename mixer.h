/*
 * standalone sound mixer based on rxi's cmixer.
 */
enum {
	FIXED_BITS	= 12,
	FIXED_UNIT	= 1<<FIXED_BITS,
	FIXED_MASK	= FIXED_UNIT-1,

	MIXBUFSIZE	= 512,
	MIXBUFMASK	= MIXBUFSIZE-1,

	AUDIO_STATE_STOPPED = 0,
	AUDIO_STATE_PLAYING,
	AUDIO_STATE_PAUSED,

	AUDIO_EVENT_DESTROY = 0,
	AUDIO_EVENT_SAMPLES,
	AUDIO_EVENT_REWIND
};

typedef struct Wav Wav;
typedef struct WavStream WavStream;
typedef struct Pcm Pcm;
typedef struct AudioEvent AudioEvent;
typedef struct AudioSourceInfo AudioSourceInfo;
typedef struct AudioSource AudioSource;
typedef struct Mixer Mixer;

struct Wav
{
	void *data;
	int bitdepth;
	int samplerate;
	int channels;
	int length;
};

struct WavStream
{
	Wav wav;
	void *data;
	int idx;
};

struct Pcm
{
	void *data;
	int len;
	int off;
	int depth;
	int chans;
	int rate;
};

struct AudioEvent
{
	int type;
	void *udata;
	char *msg;
	s16int *buffer;
	int length;
};

struct AudioSourceInfo
{
	void *udata;
	int samplerate;
	int length;

	void (*handler)(AudioEvent *e);	/* Event handler */
};

struct AudioSource
{
	AudioSource *next;		/* Next source in list */
	s16int buffer[MIXBUFSIZE];	/* Internal buffer with raw stereo PCM */
	void *udata;			/* Stream's udata (from AudioSourceInfo) */
	int samplerate;			/* Stream's native samplerate */
	int length;			/* Stream's length in samples */
	int end;			/* End index for the current play-through */
	int state;			/* Current state (playing|paused|stopped) */
	s64int position;		/* Current playhead position (fixed point) */
	int lgain, rgain;		/* Left and right gain (fixed point) */
	int rate;			/* Playback rate (fixed point) */
	int nextfill;			/* Next frame idx where the buffer needs to be filled */
	int loop;			/* Whether the source will loop when `end` is reached */
	int rewind;			/* Whether the source will rewind before playing */
	int active;			/* Whether the source is part of `sources` list */
	double gain;			/* Gain set by `audio_set_gain()` */
	double pan;			/* Pan set by `audio_set_pan()` */

	void (*handler)(AudioEvent *e);	/* Event handler */
};

struct Mixer
{
	AudioSource *sources;		/* Linked list of active (playing) sources */
	s32int buffer[MIXBUFSIZE];	/* Internal master buffer */
	int samplerate;			/* Master samplerate */
	int gain;			/* Master gain (fixed point) */
};


void audio_init(int);
void audio_set_master_gain(double);
void audio_process(s16int*, int);

AudioSource *audio_new_source(AudioSourceInfo*);
AudioSource *audio_new_source_from_file(char*);
void audio_destroy_source(AudioSource*);
double audio_get_length(AudioSource*);
double audio_get_position(AudioSource*);
int audio_get_state(AudioSource*);
void audio_set_gain(AudioSource*, double);
void audio_set_pan(AudioSource*, double);
void audio_set_pitch(AudioSource*, double);
void audio_set_loop(AudioSource*, int);
void audio_play(AudioSource*);
void audio_pause(AudioSource*);
void audio_stop(AudioSource*);
