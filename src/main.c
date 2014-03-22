#include <pebble.h>
	
#define TIMER_MS 100

enum ConfigKeys {
	CONFIG_KEY_INV=1,
	CONFIG_KEY_ANIM=2,
	CONFIG_KEY_SEP=3,
	CONFIG_KEY_DATEFMT=4,
	CONFIG_KEY_VIBR=5
};

typedef struct {
	bool inv;
	bool anim;
	bool sep;
	bool vibr;
	uint16_t datefmt;
} CfgDta_t;

static const struct GPathInfo HOUR_PATH_INFO = {
 	.num_points = 7, 
	.points = (GPoint[]) {{0, 0}, {-6, -11}, {-2, -16}, {-2, -33}, {2, -33}, {2, -16}, {6, -11}}
};
static const struct GPathInfo HOUR2_PATH_INFO = {
 	.num_points = 5, 
	.points = (GPoint[]) {{0, -2}, {-4, -11}, {-2, -14}, {2, -14}, {4, -11}}
};

static const struct GPathInfo MINS_PATH_INFO = {
 	.num_points = 7, 
	.points = (GPoint[]) {{0, 0}, {-5, -11}, {-2, -15}, {-2, -38}, {2, -38}, {2, -15}, {5, -11}}
};
static const struct GPathInfo MINS2_PATH_INFO = {
 	.num_points = 5, 
	.points = (GPoint[]) {{0, -2}, {-3, -11}, {-1, -13}, {1, -13}, {3, -11}}
};

static const struct GPathInfo SECS_PATH_INFO = {
 	.num_points = 7, 
	.points = (GPoint[]) {{0, 0}, {-4, -11}, {-2, -13}, {-2, -38}, {2, -38}, {2, -13}, {4, -11}}
};

GPath *hour_path, *mins_path, *secs_path, *hour2_path, *mins2_path;

static const uint32_t const segments[] = {100, 100, 100};
static const VibePattern vibe_pat = {
	.durations = segments,
	.num_segments = ARRAY_LENGTH(segments),
};

Window *window;
Layer *hands_layer, *secs_layer;
TextLayer* date_layer;
InverterLayer* inv_layer;
BitmapLayer *radio_layer, *battery_layer, *face_layer;

static GFont digitS;
char hhBuffer[] = "00";
char ddmmyyyyBuffer[] = "00:00 00.00.";
static GBitmap *bmp_face, *batteryAll;
static int16_t aktHH, aktMM, aktSS, step;
static AppTimer *timer;
static bool b_initialized;
static CfgDta_t CfgData;

//-----------------------------------------------------------------------------------------------------------------------
static void hands_update_proc(Layer *layer, GContext *ctx) 
{
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds), ptLin;
	graphics_context_set_stroke_color(ctx, GColorWhite);
	
	//Draw Hour Path
	int32_t angle = (TRIG_MAX_ANGLE * (((aktHH % 12) * 6) + (aktMM / 10))) / (12 * 6), sinl = sin_lookup(angle), cosl = cos_lookup(angle), rad = 36;
	ptLin.x = (int16_t)(sinl * (int32_t)(rad) / TRIG_MAX_RATIO) + center.x;
	ptLin.y = (int16_t)(-cosl * (int32_t)(rad) / TRIG_MAX_RATIO) + center.y;
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_move_to(hour_path, ptLin);
	gpath_rotate_to(hour_path, angle);
	gpath_draw_filled(ctx, hour_path);
	gpath_draw_outline(ctx, hour_path);
	graphics_context_set_fill_color(ctx, GColorBlack);
	gpath_move_to(hour2_path, ptLin);
	gpath_rotate_to(hour2_path, angle);
	gpath_draw_filled(ctx, hour2_path);

	//Draw Minute Path
	angle = TRIG_MAX_ANGLE * aktMM / 60; sinl = sin_lookup(angle); cosl = cos_lookup(angle);
	ptLin.x = (int16_t)(sinl * (int32_t)(rad) / TRIG_MAX_RATIO) + center.x;
	ptLin.y = (int16_t)(-cosl * (int32_t)(rad) / TRIG_MAX_RATIO) + center.y;
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_move_to(mins_path, ptLin);
	gpath_rotate_to(mins_path, angle);
	gpath_draw_filled(ctx, mins_path);
	gpath_draw_outline(ctx, mins_path);
	graphics_context_set_fill_color(ctx, GColorBlack);
	gpath_move_to(mins2_path, ptLin);
	gpath_rotate_to(mins2_path, angle);
	gpath_draw_filled(ctx, mins2_path);

	if (CfgData.sep)
		graphics_draw_line(ctx, GPoint(10, bounds.size.h-1), GPoint(bounds.size.w-10, bounds.size.h-1));
}
//-----------------------------------------------------------------------------------------------------------------------
static void secs_update_proc(Layer *layer, GContext *ctx) 
{
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds), ptLin;
	graphics_context_set_stroke_color(ctx, GColorWhite);
	
	//Draw Second Path
	int32_t angle = TRIG_MAX_ANGLE * aktSS / 60, sinl = sin_lookup(angle), cosl = cos_lookup(angle), rad = 36;
	ptLin.x = (int16_t)(sinl * (int32_t)(rad-14) / TRIG_MAX_RATIO) + center.x;
	ptLin.y = (int16_t)(-cosl * (int32_t)(rad-14) / TRIG_MAX_RATIO) + center.y;
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_move_to(secs_path, ptLin);
	angle = TRIG_MAX_ANGLE * ((aktSS+30)%60) / 60;
	gpath_rotate_to(secs_path, angle);
	gpath_draw_outline(ctx, secs_path);
	gpath_draw_filled(ctx, secs_path);
}
//-----------------------------------------------------------------------------------------------------------------------
static void handle_tick(struct tm *tick_time, TimeUnits units_changed) 
{
	if (b_initialized)
	{
		aktHH = tick_time->tm_hour;
		aktMM = tick_time->tm_min;
		aktSS = tick_time->tm_sec;
		
		if (aktSS == 0)
			layer_mark_dirty(hands_layer);
		layer_mark_dirty(secs_layer);
	}
	
	if (tick_time->tm_sec == 0 || units_changed == MINUTE_UNIT)
	{
		if(clock_is_24h_style())
			strftime(ddmmyyyyBuffer, sizeof(ddmmyyyyBuffer), 
				CfgData.datefmt == 1 ? "%H:%M %d-%m" : 
				CfgData.datefmt == 2 ? "%H:%M %d/%m" : 
				CfgData.datefmt == 3 ? "%H:%M %m/%d" : "%H:%M %d.%m.", tick_time);
		else
			strftime(ddmmyyyyBuffer, sizeof(ddmmyyyyBuffer), 
				CfgData.datefmt == 1 ? "%I:%M %d-%m" : 
				CfgData.datefmt == 2 ? "%I:%M %d/%m" : 
				CfgData.datefmt == 3 ? "%I:%M %m/%d" : "%I:%M %d.%m.", tick_time);
		
		text_layer_set_text(date_layer, ddmmyyyyBuffer);
	}
	
	//Hourly vibrate
	if (CfgData.vibr && tick_time->tm_min == 0 && tick_time->tm_sec == 0)
		vibes_enqueue_custom_pattern(vibe_pat); 	
}
//-----------------------------------------------------------------------------------------------------------------------
static void timerCallback(void *data) 
{
	if (!b_initialized)
	{
		time_t temp = time(NULL);
		struct tm *t = localtime(&temp);
		int16_t step_max = 20;
		
		if (step <= step_max)
		{
			aktHH = (t->tm_hour % 12)*step/step_max;
			aktMM = t->tm_min*step/step_max;
			aktSS = t->tm_sec*step/step_max;
			step++;
			
			timer = app_timer_register(TIMER_MS, timerCallback, NULL);
			layer_mark_dirty(hands_layer);
			layer_mark_dirty(secs_layer);
		}
		else
			b_initialized = true;
	}
}
//-----------------------------------------------------------------------------------------------------------------------
void battery_state_service_handler(BatteryChargeState charge_state) 
{
	int nImage = 0;
	if (charge_state.is_charging)
		nImage = 10;
	else 
		nImage = 10 - (charge_state.charge_percent / 10);
	
	GRect sub_rect = GRect(10*nImage, 0, 10*nImage+10, 20);
	bitmap_layer_set_bitmap(battery_layer, gbitmap_create_as_sub_bitmap(batteryAll, sub_rect));
}
//-----------------------------------------------------------------------------------------------------------------------
void bluetooth_connection_handler(bool connected)
{
	layer_set_hidden(bitmap_layer_get_layer(radio_layer), connected != true);
}
//-----------------------------------------------------------------------------------------------------------------------
static void update_configuration(void)
{
    if (persist_exists(CONFIG_KEY_INV))
		CfgData.inv = persist_read_bool(CONFIG_KEY_INV);
	else	
		CfgData.inv = false;
	
    if (persist_exists(CONFIG_KEY_ANIM))
		CfgData.anim = persist_read_bool(CONFIG_KEY_ANIM);
	else	
		CfgData.anim = true;
	
    if (persist_exists(CONFIG_KEY_SEP))
		CfgData.sep = persist_read_bool(CONFIG_KEY_SEP);
	else	
		CfgData.sep = true;
	
    if (persist_exists(CONFIG_KEY_DATEFMT))
		CfgData.datefmt = (int16_t)persist_read_int(CONFIG_KEY_DATEFMT);
	else	
		CfgData.datefmt = 0;
	
    if (persist_exists(CONFIG_KEY_VIBR))
		CfgData.vibr = persist_read_bool(CONFIG_KEY_VIBR);
	else	
		CfgData.vibr = false;
	
	app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "Curr Conf: inv:%d, anim:%d, sep:%d, datefmt:%d, vibr:%d",
		CfgData.inv, CfgData.anim, CfgData.sep, CfgData.datefmt, CfgData.vibr);
	
	Layer *window_layer = window_get_root_layer(window);
	//GRect bounds = layer_get_bounds(window_get_root_layer(window));
	window_set_background_color(window, GColorBlack);
	text_layer_set_text_color(date_layer, GColorWhite);
	
	layer_remove_from_parent(inverter_layer_get_layer(inv_layer));
	if (CfgData.inv)
		layer_add_child(window_layer, inverter_layer_get_layer(inv_layer));
	
	//Get a time structure so that it doesn't start blank
	time_t temp = time(NULL);
	struct tm *t = localtime(&temp);

	//Manually call the tick handler when the window is loading
	handle_tick(t, MINUTE_UNIT);

	//Set Battery state
	BatteryChargeState btchg = battery_state_service_peek();
	battery_state_service_handler(btchg);
	
	//Set Bluetooth state
	bool connected = bluetooth_connection_service_peek();
	bluetooth_connection_handler(connected);
}
//-----------------------------------------------------------------------------------------------------------------------
void in_received_handler(DictionaryIterator *received, void *ctx)
{
	app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "enter in_received_handler");
    
	Tuple *akt_tuple = dict_read_first(received);
    while (akt_tuple)
    {
        app_log(APP_LOG_LEVEL_DEBUG,
                __FILE__,
                __LINE__,
                "KEY %d=%s", (int16_t)akt_tuple->key,
                akt_tuple->value->cstring);

		if (akt_tuple->key == CONFIG_KEY_INV)
			persist_write_bool(CONFIG_KEY_INV, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_ANIM)
			persist_write_bool(CONFIG_KEY_ANIM, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_SEP)
			persist_write_bool(CONFIG_KEY_SEP, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_DATEFMT)
			persist_write_int(CONFIG_KEY_DATEFMT, 
				strcmp(akt_tuple->value->cstring, "fra") == 0 ? 1 : 
				strcmp(akt_tuple->value->cstring, "eng") == 0 ? 2 : 
				strcmp(akt_tuple->value->cstring, "usa") == 0 ? 3 : 0);
		
		if (akt_tuple->key == CONFIG_KEY_VIBR)
			persist_write_bool(CONFIG_KEY_VIBR, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		akt_tuple = dict_read_next(received);
	}
	
    update_configuration();
}
//-----------------------------------------------------------------------------------------------------------------------
void in_dropped_handler(AppMessageResult reason, void *ctx)
{
    app_log(APP_LOG_LEVEL_WARNING,
            __FILE__,
            __LINE__,
            "Message dropped, reason code %d",
            reason);
}
//-----------------------------------------------------------------------------------------------------------------------
static void window_load(Window *window) 
{
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	digitS = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_24));
	
	// Init layers
	bmp_face = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FACE);
	face_layer = bitmap_layer_create(GRect(72-34, 72-34, 68, 68));
	bitmap_layer_set_bitmap(face_layer, bmp_face);
	bitmap_layer_set_background_color(face_layer, GColorClear);
	layer_add_child(window_layer, bitmap_layer_get_layer(face_layer));
	
	hands_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.w));
	layer_set_update_proc(hands_layer, hands_update_proc);
	layer_add_child(window_layer, hands_layer);
	
	secs_layer = layer_create(GRect(72-34, 72-34, 68, 68));
	layer_set_update_proc(secs_layer, secs_update_proc);
	layer_add_child(window_layer, secs_layer);
	
	date_layer = text_layer_create(GRect(0, bounds.size.w-3, bounds.size.w, bounds.size.h-bounds.size.w+3));
	text_layer_set_background_color(date_layer, GColorClear);
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
	text_layer_set_font(date_layer, digitS);
	layer_add_child(window_layer, text_layer_get_layer(date_layer));

	//Init battery
	batteryAll = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
	battery_layer = bitmap_layer_create(GRect(bounds.size.w-11, bounds.size.h-21, 10, 20)); 
	bitmap_layer_set_background_color(battery_layer, GColorClear);
	layer_add_child(window_layer, bitmap_layer_get_layer(battery_layer));

	//Init bluetooth radio
	radio_layer = bitmap_layer_create(GRect(0, bounds.size.h-21, 10, 20));
	bitmap_layer_set_background_color(radio_layer, GColorClear);
	bitmap_layer_set_bitmap(radio_layer, gbitmap_create_as_sub_bitmap(batteryAll, GRect(110, 0, 10, 20)));
	layer_add_child(window_layer, bitmap_layer_get_layer(radio_layer));
	
	//Init Inverter Layer
	inv_layer = inverter_layer_create(bounds);	
	
	//Update Configuration
	update_configuration();
	
	//Start|Skip Animation
	if (CfgData.anim)
	{
		step = aktHH = aktMM = aktSS = 0;
		timerCallback(NULL);
	}	
	else
		b_initialized = true;
}
//-----------------------------------------------------------------------------------------------------------------------
static void window_unload(Window *window) 
{
	layer_destroy(secs_layer);
	layer_destroy(hands_layer);
	text_layer_destroy(date_layer);
	bitmap_layer_destroy(battery_layer);
	bitmap_layer_destroy(radio_layer);
	bitmap_layer_destroy(face_layer);
	inverter_layer_destroy(inv_layer);
	fonts_unload_custom_font(digitS);
	gbitmap_destroy(batteryAll);
	gbitmap_destroy(bmp_face);
	if (!b_initialized)
		app_timer_cancel(timer);
}
//-----------------------------------------------------------------------------------------------------------------------
static void init(void) 
{
	b_initialized = false;

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	// Init paths
	hour_path = gpath_create(&HOUR_PATH_INFO);
	mins_path = gpath_create(&MINS_PATH_INFO);
	secs_path = gpath_create(&SECS_PATH_INFO);
	hour2_path = gpath_create(&HOUR2_PATH_INFO);
	mins2_path = gpath_create(&MINS2_PATH_INFO);
	
	// Push the window onto the stack
	window_stack_push(window, true);
	
	//Subscribe ticks
	tick_timer_service_subscribe(SECOND_UNIT, handle_tick);

	//Subscribe smart status
	battery_state_service_subscribe(&battery_state_service_handler);
	bluetooth_connection_service_subscribe(&bluetooth_connection_handler);
	
	//Subscribe messages
	app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_open(128, 128);
}
//-----------------------------------------------------------------------------------------------------------------------
static void deinit(void) 
{
	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	
	gpath_destroy(hour_path);
	gpath_destroy(mins_path);
	gpath_destroy(secs_path);
	gpath_destroy(hour2_path);
	gpath_destroy(mins2_path);
	
	window_destroy(window);
}
//-----------------------------------------------------------------------------------------------------------------------
int main(void) 
{
	init();
	app_event_loop();
	deinit();
}
//-----------------------------------------------------------------------------------------------------------------------