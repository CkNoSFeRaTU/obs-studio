#include <inttypes.h>
#include <signal.h>
#include <unistd.h>

#include "graphics/matrix4.h"
#include <graphics/vec2.h>
#include "callback/calldata.h"

#include "obs.h"
#include "obs-internal.h"

#include "list.h"

#include <jansson.h>

typedef struct
{
	bool is_shutdown;

	char *render;
	int scene_width;
	int scene_height;

	obs_scene_t *scene;

	list_t *encoders;
	list_t *sources;

	json_t *json;

	struct obs_audio_info ai;
	struct obs_video_info ovi;
} obscli_t;

typedef struct
{
	int video_width;
	int video_height;

	int video_bitrate;
	int audio_bitrate;
	char *preset;
	char *profile;

	obs_encoder_t *audio_encoder;
	obs_encoder_t *video_encoder;

	list_t *outputs;
} obscli_encoder_t;

typedef struct
{
	char *type;
	char *server;
	char *key;

	obs_service_t *service;

	obs_output_t *output;
} obscli_encoder_output_t;

obscli_t *obscli;

static void shut_obscli(obscli_t *obscli)
{
	size_t i;

	blog(LOG_INFO, "Shutting down...");

	list_t *source_item = list_get_first(obscli->sources);
	while (source_item) {
		obs_sceneitem_t *item = list_get_data(source_item);

		obs_sceneitem_release(item);

		obscli->sources = list_delete(obscli->sources, source_item);

		source_item = list_get_next(source_item);
	}

	list_t *encoder_item = list_get_first(obscli->encoders);
	while (encoder_item) {
		obscli_encoder_t *encoder = list_get_data(encoder_item);
		list_t *output_item = list_get_first(encoder->outputs);
		while (output_item) {
			obscli_encoder_output_t *output = list_get_data(output_item);

			obs_output_stop(output->output);
//			obs_output_release(output->output);
			obs_service_release(output->service);

			encoder->outputs = list_delete(encoder->outputs, output_item);

			output_item = list_get_next(output_item);
		}

		obs_encoder_release(encoder->video_encoder);
		obs_encoder_release(encoder->audio_encoder);

		obscli->encoders = list_delete(obscli->encoders, encoder_item);

		encoder_item = list_get_next(encoder_item);
	}

	obs_scene_release(obscli->scene);

	profiler_stop();
	profiler_print(0);
	profiler_free();

	json_decref(obscli->json);
	obscli->json = NULL;

//	obs_shutdown();

	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());
}

static int init_obscli(obscli_t *obscli)
{
	profiler_start();
	profile_register_root("this", 0);
	if (!obs_startup("en-US", NULL, NULL))
	{
		blog(LOG_ERROR, "Couldn't create OBS");
		return -1;
	}
	blog(LOG_INFO, "Core Version: %lu", (long unsigned int)obs_get_version());

	obs_load_all_modules();
	obs_log_loaded_modules();

	obscli->ovi.adapter = 0;
	obscli->ovi.base_width = obscli->scene_width;
	obscli->ovi.base_height = obscli->scene_height;
	obscli->ovi.fps_num = 25;
	obscli->ovi.fps_den = 1;
	obscli->ovi.graphics_module = obscli->render;
	obscli->ovi.output_format = VIDEO_FORMAT_RGBA;
	obscli->ovi.output_width = obscli->scene_width;
	obscli->ovi.output_height = obscli->scene_height;
	// TODO: move scaling
	obscli->ovi.scale_type = OBS_SCALE_BILINEAR;

	if (obs_reset_video(&obscli->ovi) != 0)
	{
		blog(LOG_ERROR, "Couldn't initialize video");
		return -1;
	}

	obscli->ai.samples_per_sec = 44100;
	obscli->ai.speakers = SPEAKERS_STEREO;
	if (obs_reset_audio(&obscli->ai) != true)
	{
		blog(LOG_ERROR, "Couldn't initialize audio");
		return -1;
	}

	obscli->scene = obs_scene_create("main_scene");
}

static int init_obscli_encoders(obscli_t *obscli)
{
	list_t *encoder_item = list_get_first(obscli->encoders);
	while (encoder_item) {
		obscli_encoder_t *encoder = list_get_data(encoder_item);

		if (encoder->video_bitrate > 0) {
			encoder->video_encoder = obs_video_encoder_create("obs_x264", "x264 encoder", NULL, NULL);

			obs_encoder_set_scaled_size(encoder->video_encoder, encoder->video_width, encoder->video_height);
			obs_encoder_set_preferred_video_format(encoder->video_encoder, VIDEO_FORMAT_I420);

			obs_data_t *video_settings = obs_data_create();
			obs_data_set_int(video_settings, "bitrate", encoder->video_bitrate);
			obs_data_set_int(video_settings, "keyint_sec", 1);
			obs_data_set_string(video_settings, "preset", encoder->preset);
			obs_data_set_string(video_settings, "profile", encoder->profile);
			obs_encoder_update(encoder->video_encoder, video_settings);
			obs_data_release(video_settings);
		}

		if (encoder->audio_bitrate > 0) {
			encoder->audio_encoder = obs_audio_encoder_create("ffmpeg_aac", "aac encoder", NULL, 0, NULL);

			obs_data_t *audio_settings = obs_data_create();
			obs_data_set_int(audio_settings, "bitrate", encoder->audio_bitrate);
			obs_data_set_bool(audio_settings, "cbr", true);

			obs_encoder_update(encoder->audio_encoder, audio_settings);
			obs_data_release(audio_settings);
		}

		list_t *output_item = list_get_first(encoder->outputs);
		while (output_item) {
			obscli_encoder_output_t *output = list_get_data(output_item);

			if (strcmp(output->type, "rtmp_output") == 0) {
				output->output = obs_output_create(output->type, "", NULL, NULL);
				obs_output_set_video_encoder(output->output, encoder->video_encoder);
				obs_output_set_audio_encoder(output->output, encoder->audio_encoder, 0);

				obs_encoder_set_video(encoder->video_encoder, obs_get_video());
				obs_encoder_set_audio(encoder->audio_encoder, obs_get_audio());

				obs_data_t *service_settings = obs_data_create();
				obs_data_set_string(service_settings, "server", output->server);
				obs_data_set_string(service_settings, "key", output->key);
				output->service = obs_service_create("rtmp_common", "default_service", service_settings, NULL);
				obs_output_set_service(output->output, output->service);
				obs_output_set_delay(output->output, 0, 0);
				obs_output_set_mixer(output->output, 0);
				if (obs_output_start(output->output))
				{
					blog(LOG_INFO, "translation started...");
				} else {
					blog(LOG_INFO, "translation could not be started...");
					return 0;
				}
			} else {
				blog(LOG_ERROR, "currently unsupported output\n");
				return 0;
			}

			output_item = list_get_next(output_item);
		}

		encoder_item = list_get_next(encoder_item);
	}

	obs_set_output_source(0, obs_scene_get_source(obscli->scene));
	
	return 1;
}

static void add_obscli_scene_parameters(obs_data_t *settings, json_t *parameters)
{
	size_t i;

	for(i = 0; i < json_array_size(parameters); i++) {
		const char *name = json_string_value(json_object_get(json_array_get(parameters, i), "name"));
		json_t *value = (json_object_get(json_array_get(parameters, i), "value"));
		if (json_is_string(value))
			obs_data_set_string(settings, name, json_string_value(value));
		else if (json_is_integer(value))
			obs_data_set_int(settings, name, json_integer_value(value));
		else if (json_is_boolean(value))
			obs_data_set_bool(settings, name, json_boolean_value(value));
		else if (json_is_array(value)) {
			obs_data_array_t *obs_array = obs_data_array_create();
			obs_data_t *obs_array_item = obs_data_create();

			add_obscli_scene_parameters(obs_array_item, value);

			obs_data_array_insert(obs_array, 0, obs_array_item);
			obs_data_set_array(settings, name, obs_array);
		}
	}
}

static int add_obscli_scene(obscli_t *obscli, char *type, json_t *parameters, char *scale, int pos_x, int pos_y, int width, int height, float volume)
{
	struct obs_transform_info info;
	obs_data_t *settings = obs_data_create();

	add_obscli_scene_parameters(settings, parameters);

	obs_source_t *source = obs_source_create(type, "", settings, NULL);

        obs_data_release(settings);
	obs_source_set_volume(source, volume);

	obs_sceneitem_t *item = obs_scene_add(obscli->scene, source);

	obs_sceneitem_get_info(item, &info);
	if (strcmp(scale, "inner") == 0)
		info.bounds_type = OBS_BOUNDS_SCALE_INNER;
	else if (strcmp(scale, "stretch") == 0)
		info.bounds_type = OBS_BOUNDS_STRETCH;
	else
		info.bounds_type = OBS_BOUNDS_NONE;
	if (width > 0 || height > 0)
		vec2_set(&info.bounds, width, height);
	if (pos_x > 0 || pos_y > 0)
		vec2_set(&info.pos, pos_x, pos_y);

	obs_sceneitem_set_info(item, &info);

	obscli->sources = list_append(obscli->sources, item);

	return 0;
}

static int load_config(obscli_t *obscli, char *path)
{
	size_t i, io;
	json_error_t error; 

	obscli->json = json_load_file(path, 0, &error);
	if (!obscli->json) {
		blog(LOG_ERROR, "config error: on line %d: %s\n", error.line, error.text);
		return 0;
	}

	json_t *settings = json_object_get(obscli->json, "settings");
	if(!json_is_object(settings)) {
		json_decref(settings);
		json_decref(obscli->json);
		return 0;
	}
	obscli->render = (char *)json_string_value(json_object_get(settings, "graphics"));
	obscli->scene_width = json_integer_value(json_object_get(settings, "width"));
	obscli->scene_height = json_integer_value(json_object_get(settings, "height"));

	json_t *encoders = json_object_get(obscli->json, "encoders");
	if(!json_is_array(encoders)) {
		json_decref(encoders);
		json_decref(obscli->json);
		return 0;
	}

	for(i = 0; i < json_array_size(encoders); i++) {
		int outputs_count = 0;
		json_t *cur_encoder = json_array_get(encoders, i);

		obscli_encoder_t *encoder = calloc(1, sizeof(obscli_encoder_t));

		encoder->video_width = json_integer_value(json_object_get(cur_encoder, "width"));
		encoder->video_height = json_integer_value(json_object_get(cur_encoder, "height"));
		encoder->audio_bitrate = json_integer_value(json_object_get(cur_encoder, "audio_bitrate"));
		encoder->video_bitrate = json_integer_value(json_object_get(cur_encoder, "video_bitrate"));
		encoder->preset = (char *)json_string_value(json_object_get(cur_encoder, "preset"));
		encoder->profile = (char *)json_string_value(json_object_get(cur_encoder, "profile"));

		json_t *outputs = json_object_get(cur_encoder, "outputs");
		if(!json_is_array(outputs)) {
			json_decref(outputs);
			json_decref(obscli->json);
			return 0;
		}

		for(io = 0; io < json_array_size(outputs); io++) {
			json_t *cur_output = json_array_get(outputs, io);
			obscli_encoder_output_t *output = calloc(1, sizeof(obscli_encoder_output_t));

			output->type = (char *)json_string_value(json_object_get(cur_output, "type"));
			output->server = (char *)json_string_value(json_object_get(cur_output, "server"));
			output->key = (char *)json_string_value(json_object_get(cur_output, "key"));

			encoder->outputs = list_append(encoder->outputs, output);

			outputs_count++;
		}
		
		if (outputs_count > 0)
			obscli->encoders = list_append(obscli->encoders, encoder);
		else
			free(encoder);
	}

	if (init_obscli(obscli) < 0)
		return 0;
	init_obscli_encoders(obscli);

	json_t *sources = json_object_get(obscli->json, "sources");
	if(!json_is_array(sources)) {
		json_decref(sources);
		json_decref(obscli->json);
		return 0;
	}
	for(i = 0; i < json_array_size(sources); i++) {
		char *source_type = (char *)json_string_value(json_object_get(json_array_get(sources, i), "type"));
		char *source_scale = (char *)json_string_value(json_object_get(json_array_get(sources, i), "scale"));
		int source_pos_x = json_integer_value(json_object_get(json_array_get(sources, i), "pos_x"));
		int source_pos_y = json_integer_value(json_object_get(json_array_get(sources, i), "pos_y"));
		int source_width = json_integer_value(json_object_get(json_array_get(sources, i), "width"));
		int source_height = json_integer_value(json_object_get(json_array_get(sources, i), "height"));
		float volume = json_integer_value(json_object_get(json_array_get(sources, i), "volume")) / 100.0f;

		add_obscli_scene(obscli, source_type, json_object_get(json_array_get(sources, i), "parameters"), source_scale, source_pos_x, source_pos_y, source_width, source_height, volume);
	}

	return 1;
}

void sig_handler(int sig)
{
	obscli->is_shutdown = true;
}

int main(int argc, char *argv[])
{

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	if (argc < 2) {
		blog(LOG_ERROR, "please run with following arguments: %s <config>", argv[0]);
		return -1;
	}

	obscli = calloc(1, sizeof(obscli_t));
	if (!load_config(obscli, argv[1])) {
		blog(LOG_ERROR, "config loading failed!");
		return -1;
	}

	do
	{
		pause();
	} while (!obscli->is_shutdown);

	shut_obscli(obscli);

	return 0;
}
