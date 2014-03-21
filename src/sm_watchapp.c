#include <pebble.h>
#include "globals.h"

static Window *window;

//#define STRING_LENGTH 255
#define DATE_STRING_LENGTH  30
#define CAL_TEXT_STRING_LENGTH  25
#define MUSIC_TEXT_STRING_LENGTH  30
#define WEATHER_DAY_STRING_LENGTH 20
#define BADGE_COUNT_STRING_LENGTH 6
#define WIND_STRING_LENGTH 20
#define HUMIDITY_STRING_LENGTH 16
#define NUM_WEATHER_IMAGES	15
#define NUM_MOON_IMAGES	8
#define WEATHER_COND_STRING_LENGTH 20
#define FS_CONFIG_KEY_THEME 0xFC49

typedef enum { WEATHER_LAYER, WEATHER_LAYER2, MOON_LAYER, CALENDAR_LAYER, MUSIC_LAYER, NUM_LAYERS } AnimatedLayers;
typedef enum { STATUS_LAYER, CALENDAR_STATUS_LAYER, NUM_STATUS_LAYERS } StatusLayers;
typedef enum { NEW_MOON, WAXING_CRESCENT, FIRST_QUARTER, WAXING_GIBBOUS, FULL_MOON, WANING_GIBBOUS, LAST_QUARTER, WANING_CRESCENT } MoonPhases;
typedef enum { HUM_0, HUM_25, HUM_50, HUM_75, HUM_100 } HumidityLevels;

// Modes for Views
typedef enum { WEATHER_CURRENT, WEATHER_DAY1, WEATHER_DAY2, WEATHER_DAY3, NUM_WEATHER_MODES } WeatherDisplayModes;
typedef enum { APPT_1, APPT_2, APPT_3, APPT_4, NUM_APPT_MODES } AppointmentModes;
//typedef enum { APPT_1, APPT_2, APPT_3, NUM_APPT_MODES } AppointmentModes;
typedef enum { MOON_CURRENT, MOON_NEXT_NEW, MOON_NEXT_FULL, NUM_MOON_MODES } MoonModes;

static char *moon_phase_names[] = { "New Moon", "Waxing Cres", "First Qtr", "Waxing Gibb", "Full Moon", "Waning Gibb", "Last Qtr", "Waning Cres" };
static char *default_cal_names[] = { "No Upcoming", "Appointment" };

static int active_layer = 0;
static int active_status_layer = 0;
static bool response_mode_active = false;

static int prev_sms_count = -1;
static int prev_call_count = -1;
static int prev_active_layer = -1;
static int data_mode = STATUS_SCREEN_APP;
static int response_mode_timeout = 90000; // 90 sec
//static int wait_for_msg_app_mode = 0;

static const int NEXT_ITEM = -1;
static const int MIDDLE_LAYERS = 0;
static const int BOTTOM_LAYERS = 1;

static PropertyAnimation *ani_out = NULL, *ani_in = NULL;
static PropertyAnimation *ani_out_status = NULL, *ani_in_status = NULL;

static char *layer_names[] = { "", "Weather Data", "", "", "Music" };
static char *status_layer_names[] = { "Phone Data", "" };
static char *updating_str = "Updating...";

static char *weather_mode_names[] = { "Current Weather", "Today's Weather", "Tomorrow", "Upcoming" };
static char *appt_mode_names[] = { "First Appt", "Second Appt", "Third Appt", "Fourth Appt" };
static char *moon_mode_names[] = { "Moon Data", "Next %s Moon" };

static int active_weather_mode_index = 0;
static int active_appt_mode_index = 0;
static int active_appt_status_mode_index = 0;
static int active_moon_mode_index = 0;

static int last_weather_img_set = 0;
static int last_humidity_img_set = HUM_50;

static AppTimer* dateRecoveryTimer = NULL;
static AppTimer* responseModeTimer = NULL;
static int date_switchback_short = 2000;
static int date_switchback_long = 5000;

static TextLayer *text_weather_cond_layer, *text_weather_temp_layer;
static TextLayer *text_weather_wind_layer, *text_weather_humidity_layer, *text_weather_humidity_label_layer, *text_weather_wind_label_layer, *text_weather_hi_lo_label_layer;
static TextLayer *text_weather_day2_layer, *text_weather_day3_layer;
static TextLayer *text_weather_day2_cond_layer, *text_weather_day3_cond_layer;
static TextLayer *moon_text_layer, *moon_phase_layer;
static TextLayer *text_date_layer, *text_time_layer;
static TextLayer *text_mail_layer, *text_sms_layer, *text_phone_layer, *text_battery_layer;
static TextLayer *calendar_date_layer1, *calendar_text_layer1;
static TextLayer *calendar_date_layer2, *calendar_text_layer2;
static TextLayer *music_artist_layer, *music_song_layer;

static Layer *battery_layer, *pebble_battery_layer;
static BitmapLayer *background_image, *weather_image, *moon_image, *status_image, *wind_image, *humidity_image;

static Layer *status_layer[NUM_STATUS_LAYERS], *animated_layer[NUM_LAYERS];

static int active_layer, active_status_layer;

static char battery_pct_str[5];
static char weather_wind_str[WIND_STRING_LENGTH], weather_humidity_str[HUMIDITY_STRING_LENGTH];
static char sms_count_str[BADGE_COUNT_STRING_LENGTH], mail_count_str[BADGE_COUNT_STRING_LENGTH], phone_count_str[BADGE_COUNT_STRING_LENGTH];
static int weather_img, batteryPercent, batteryPblPercent;

static char weather_cond_str[NUM_WEATHER_MODES][WEATHER_COND_STRING_LENGTH];
static char weather_temp_or_day_str[NUM_WEATHER_MODES][WEATHER_DAY_STRING_LENGTH];
static char* p_weather_hi_lo_str[NUM_WEATHER_MODES];
static int weather_icons[NUM_WEATHER_MODES];

static char moon_top_text_str[NUM_MOON_MODES-1][18];
static char moon_bottom_text_str[NUM_MOON_MODES-1][18];
static int moon_icons[NUM_MOON_MODES-1];


static char calendar_text_str[NUM_APPT_MODES][CAL_TEXT_STRING_LENGTH], calendar_date_str[NUM_APPT_MODES][DATE_STRING_LENGTH];
static char old_date_str[DATE_STRING_LENGTH];
static char music_artist_str[MUSIC_TEXT_STRING_LENGTH], music_title_str[MUSIC_TEXT_STRING_LENGTH];
static char response_mode_top_str[MUSIC_TEXT_STRING_LENGTH], response_mode_bottom_str[MUSIC_TEXT_STRING_LENGTH];
static int actual_num_appt_modes = NUM_APPT_MODES;

static bool color_mode_normal = true;

GBitmap *bg_image;
GBitmap *current_status_image;
GBitmap *current_weather_image;
GBitmap *current_moon_image;
GBitmap *current_wind_image;
GBitmap *current_humidity_image;


const int INTERNAL_UPDATE_TIME = 20;
static int current_update_min_hits = 0;
static bool first_timer_tick = false;

static char *weather_conditions[] = { "Clear", "Rain", "Cloudy", "Partly Cloudy", "Foggy", "Windy", "Snow", "Thunderstorms" };

const int WEATHER_IMG_IDS[] = {
	RESOURCE_ID_IMAGE_SUN, 		// 0
	RESOURCE_ID_IMAGE_RAIN,		// 1
	RESOURCE_ID_IMAGE_CLOUD,		// 2
	RESOURCE_ID_IMAGE_SUN_CLOUD,	// 3
	RESOURCE_ID_IMAGE_FOG,		// 4
	RESOURCE_ID_IMAGE_WIND,		// 5
	RESOURCE_ID_IMAGE_SNOW,		// 6
	RESOURCE_ID_IMAGE_THUNDER,	// 7
	RESOURCE_ID_IMAGE_MOON,		// 8
	RESOURCE_ID_IMAGE_MOON_CLOUDS,// 9
	RESOURCE_ID_IMAGE_SUN_FOG,	// 10
	RESOURCE_ID_IMAGE_MOON_FOG,	// 11
	RESOURCE_ID_IMAGE_SUN_WIND,	// 12
	RESOURCE_ID_IMAGE_MOON_WIND,	// 13	
	RESOURCE_ID_IMAGE_DISCONNECT	// 14
};

const int WEATHER_IMG_INV_IDS[] = {
	RESOURCE_ID_IMAGE_SUN_INV,
	RESOURCE_ID_IMAGE_RAIN_INV,
	RESOURCE_ID_IMAGE_CLOUD_INV,
	RESOURCE_ID_IMAGE_SUN_CLOUD_INV,
	RESOURCE_ID_IMAGE_FOG_INV,
	RESOURCE_ID_IMAGE_WIND_INV,
	RESOURCE_ID_IMAGE_SNOW_INV,
	RESOURCE_ID_IMAGE_THUNDER_INV,
	RESOURCE_ID_IMAGE_MOON_INV,
	RESOURCE_ID_IMAGE_MOON_CLOUDS_INV,
	RESOURCE_ID_IMAGE_SUN_FOG_INV,
	RESOURCE_ID_IMAGE_MOON_FOG_INV,
	RESOURCE_ID_IMAGE_SUN_WIND_INV,
	RESOURCE_ID_IMAGE_MOON_WIND_INV,
	RESOURCE_ID_IMAGE_DISCONNECT_INV
};

const int HUMIDITY_IMG_IDS[] = {
	RESOURCE_ID_IMAGE_HUMIDITY_0,
	RESOURCE_ID_IMAGE_HUMIDITY_25,
	RESOURCE_ID_IMAGE_HUMIDITY_50,
	RESOURCE_ID_IMAGE_HUMIDITY_75,
	RESOURCE_ID_IMAGE_HUMIDITY_100
};

const int HUMIDITY_IMG_INV_IDS[] = {
	RESOURCE_ID_IMAGE_HUMIDITY_0_INV,
	RESOURCE_ID_IMAGE_HUMIDITY_25_INV,
	RESOURCE_ID_IMAGE_HUMIDITY_50_INV,
	RESOURCE_ID_IMAGE_HUMIDITY_75_INV,
	RESOURCE_ID_IMAGE_HUMIDITY_100_INV
};

const int MOON_IMG_IDS[] = {
	RESOURCE_ID_IMAGE_MOON_NEW,
	RESOURCE_ID_IMAGE_MOON_WAXING_CRESCENT,
	RESOURCE_ID_IMAGE_MOON_FIRST_QTR,
	RESOURCE_ID_IMAGE_MOON_WAXING_GIB,
	RESOURCE_ID_IMAGE_MOON_FULL,
	RESOURCE_ID_IMAGE_MOON_WANING_GIB,
	RESOURCE_ID_IMAGE_MOON_LAST_QTR,
	RESOURCE_ID_IMAGE_MOON_WANING_CRESCENT
};

const int MOON_IMG_INV_IDS[] = {
	RESOURCE_ID_IMAGE_MOON_NEW_INV,
	RESOURCE_ID_IMAGE_MOON_WAXING_CRESCENT_INV,
	RESOURCE_ID_IMAGE_MOON_FIRST_QTR_INV,
	RESOURCE_ID_IMAGE_MOON_WAXING_GIB_INV,
	RESOURCE_ID_IMAGE_MOON_FULL_INV,
	RESOURCE_ID_IMAGE_MOON_WANING_GIB_INV,
	RESOURCE_ID_IMAGE_MOON_LAST_QTR_INV,
	RESOURCE_ID_IMAGE_MOON_WANING_CRESCENT_INV
};

// Prototypes
static void set_info_text_with_timer(char *, int);
static void transition_main_layer(int);
static void handle_calendar_mode_click(int);
static void handle_calendar_status_mode_click(int);
void set_moon_data();
void dismiss_response_mode();
static void handle_weather_mode_click(int);
static void deinit();
static void init();

static uint32_t s_sequence_number = 0xFFFFFFFE;

AppMessageResult sm_message_out_get(DictionaryIterator **iter_out) {
	AppMessageResult result = app_message_outbox_begin(iter_out);
	if (result != APP_MSG_OK) {
		return result;
	}
	dict_write_int32(*iter_out, SM_SEQUENCE_NUMBER_KEY, ++s_sequence_number);
	if (s_sequence_number == 0xFFFFFFFF) {
		s_sequence_number = 1;
	}
	return APP_MSG_OK;
}

static int moonPhase() {
	time_t tt = time(NULL);
	struct tm * tm_now = localtime(&tt);
	
	int year = tm_now->tm_year + 1900;
	int month = tm_now->tm_mon + 1;
	int day = tm_now->tm_mday;
	
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Time %d / %d / %d", month, day, year);
	
	int r = year % 100;
	r %= 19;
	if (r>9){ r -= 19;}
	r = ((r * 11) % 30) + month + day;
	if (month<3){r += 2;}
	r -= ((year<2000) ? 4 : 8.3);
	r = ((int)(r+0.5))%30;
	int ret = (r < 0) ? r+30 : r;
	//ret -= 1;
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "mp %d", (int)ret);
	
	return (int)ret;
}

static time_t get_next_new_moon_date(){
	
	int daysLeft = (int)(29.53059 - (double)moonPhase());
	if (daysLeft < 0) {
		daysLeft = 0;
	}
	
	// testing
	//time_t fakenow = (time(NULL) + (18 * 86400));
	//time_t newMoon = fakenow + ((daysLeft+1) * 86400);
	time_t newMoon = time(NULL) + ((daysLeft+1) * 86400);

	return newMoon;
}

static time_t get_next_full_moon_date(){
	
	int daysLeft = (int)(14.76529 - (double)moonPhase());
	if (daysLeft < 0) {
		daysLeft += 29.53059;
	}
	
	// testing
	//time_t fakenow = (time(NULL) + (18 * 86400));
	//time_t newMoon = fakenow + ((daysLeft+1) * 86400);
	time_t fullMoon = time(NULL) + ((daysLeft+1) * 86400);

	return fullMoon;
}


static bool isDay() {
	time_t tt = time(NULL);
	struct tm * stm = localtime(&tt);

	if (stm->tm_hour >= 19 || stm->tm_hour < 7) {
		return false;
	}
	return true;
}

void get_date_back(void *data) {
	dateRecoveryTimer = NULL;
	text_layer_set_text(text_date_layer, old_date_str);
	
	handle_weather_mode_click(WEATHER_CURRENT);
}

void set_timer_for_date_recovery(int time_len) {
	if (dateRecoveryTimer != NULL) {
		app_timer_reschedule(dateRecoveryTimer, time_len);
	} else {
		dateRecoveryTimer = app_timer_register(time_len, get_date_back, NULL);
	}
}

void reset_sequence_number() {
	DictionaryIterator *iter = NULL;
	app_message_outbox_begin(&iter);
	if (!iter) return;
	dict_write_int32(iter, SM_SEQUENCE_NUMBER_KEY, 0xFFFFFFFF);
	app_message_outbox_send();
}


void sendCommand(int key) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
	if (!iterout) return;

	dict_write_int8(iterout, key, -1);
	app_message_outbox_send();
}


void sendCommandInt(int key, int param) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
	if (!iterout) {
		return;
	}

	dict_write_int8(iterout, key, param);
	app_message_outbox_send();
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "app_message_outbox_send");
}

static void set_status_app_as_active(void *data) {
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
}

void data_update_and_refresh() {
	// This is a hack.  When you query the weather for humidity and wind, those tuple
	// vals will in turn send out a normal SS messgae for update.  You can't send two at the same time,
	// so this is hopefully a temporary hack.
	data_mode = WEATHER_APP;
	sendCommandInt(SM_SCREEN_ENTER_KEY, WEATHER_APP);
	app_timer_register(20000, set_status_app_as_active, NULL);
	//sendCommandInt(SM_SCREEN_ENTER_KEY, MESSAGES_APP);
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "MESSAGES_APP SENT...");
}


void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	// Need to be static because they're used by the system later.
	static char time_text[] = "00:00";
	static char date_text[] = "Xxxxxxxxx 00";

	char *time_format;

	// TODO: Only update the date when it's changed.
	strftime(date_text, sizeof(date_text), "%a, %b %e", tick_time);
	
	if (!response_mode_active)
		text_layer_set_text(text_date_layer, date_text);


	if (clock_is_24h_style()) {
		time_format = "%R";
	}
	else  {
		time_format = "%I:%M";
	}

	strftime(time_text, sizeof(time_text), time_format, tick_time);

	// Kludge to handle lack of non-padded hour format string
	// for twelve hour clock.
	if (!clock_is_24h_style() && (time_text[0] == '0')) {
		memmove(time_text, &time_text[1], sizeof(time_text)-1);
	}

	text_layer_set_text(text_time_layer, time_text);

	strcpy(old_date_str, date_text);
	
	// Internal update request stuff
	++current_update_min_hits;

	if (first_timer_tick == false || current_update_min_hits >= INTERNAL_UPDATE_TIME) {
		current_update_min_hits = 0;
		set_moon_data();
		data_update_and_refresh();
	}

	//if (tick_time->tm_hour == hour_white && tick_time->tm_min == minute_white && color_mode_normal == 1) {
	//	deinit();
	//	color_mode_normal = 0;
	//	init();
	//	text_layer_set_text(text_weather_cond_layer, "Updating..." ); 		
	//	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	//} else if (tick_time->tm_hour == hour_black && tick_time->tm_min == minute_black && color_mode_normal == 0) {
	//	deinit();
	//	color_mode_normal = 1;
	//	init();
	//	text_layer_set_text(text_weather_cond_layer, "Updating..." ); 		
	//	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	//}

	first_timer_tick = true;
}

static int get_weather_image_index(int index, int condIndx) {
	int wImg = index;
	
	if (wImg == 0) { // Sun icon
		if (!isDay()) {
			wImg = 8;
		}
	}
	else if (wImg == 3) { // partly cloudy
		if (!isDay()) {
			wImg = 9;
		}
	}
	else if (wImg == 4) { // fog
		if (isDay()) {
			wImg = 10;
		}
		else {
			wImg = 11;
		}
	}
	else if (wImg == 5) { // wind
		if (isDay()) {
			wImg = 12;
		}
		else {
			wImg = 13;
		}
	}
	last_weather_img_set = wImg;
	return wImg;
}

static int get_moon_phase(int age) {
	if (age <= 1) {
		return NEW_MOON; // new moon
	}
	if (age <= 6) {
		return WAXING_CRESCENT; // waxing crescent
	}
	if (age <= 9) {
		return FIRST_QUARTER; // first quarter
	}
	if (age <= 13) {
		return WAXING_GIBBOUS; // waxing gibbous
	}
	if (age <= 16) {
		return FULL_MOON; // full moon
	}
	if (age <= 20) {
		return WANING_GIBBOUS; // waning gibbous
	}
	if (age <= 23) {
		return LAST_QUARTER; // Last quarter
	}
	if (age <= 28) {
		return WANING_CRESCENT; // waning crescent
	}

	return NEW_MOON; // new moon
}

void setMoonImage(int index) {
	gbitmap_destroy(current_moon_image);
	if (color_mode_normal == true) {
		current_moon_image = gbitmap_create_with_resource(MOON_IMG_IDS[index]);
	}
	else {
		current_moon_image = gbitmap_create_with_resource(MOON_IMG_INV_IDS[index]);
	}

	bitmap_layer_set_bitmap(moon_image, current_moon_image);
}

void setWeatherImage(int index) {
	gbitmap_destroy(current_weather_image);
	if (color_mode_normal == true) {
		current_weather_image = gbitmap_create_with_resource(WEATHER_IMG_IDS[index]);
	}
	else {
		current_weather_image = gbitmap_create_with_resource(WEATHER_IMG_INV_IDS[index]);
	}

	bitmap_layer_set_bitmap(weather_image, current_weather_image);
}

void setHumidityImage(int index) {
	gbitmap_destroy(current_humidity_image);
	if (color_mode_normal == true) {
		current_humidity_image = gbitmap_create_with_resource(HUMIDITY_IMG_IDS[index]);
	}
	else {
		current_humidity_image = gbitmap_create_with_resource(HUMIDITY_IMG_INV_IDS[index]);
	}

	bitmap_layer_set_bitmap(humidity_image, current_humidity_image);
}

void set_moon_data() {
	int mDays = moonPhase();
	snprintf(moon_top_text_str[MOON_CURRENT], 18, "Age: %d days", mDays);
	
	text_layer_set_text(moon_text_layer, moon_top_text_str[MOON_CURRENT]);

	int moonPhase = get_moon_phase(mDays);
	setMoonImage(moonPhase);
	moon_icons[MOON_CURRENT] = moonPhase;
	text_layer_set_text(moon_phase_layer, moon_phase_names[moonPhase]);
}

void format_and_set_moon_date(time_t tt) {
	struct tm * nm = localtime(&tt);
	strftime(moon_bottom_text_str[MOON_NEXT_NEW], sizeof(moon_bottom_text_str[MOON_NEXT_NEW]), "%a, %b %e", nm);
	text_layer_set_text(moon_phase_layer, moon_bottom_text_str[MOON_NEXT_NEW]);
	text_layer_set_text(moon_text_layer, moon_top_text_str[MOON_NEXT_NEW]);
	setMoonImage(moon_icons[MOON_NEXT_NEW]);
}

void set_new_moon_data() {
	moon_icons[MOON_NEXT_NEW] = NEW_MOON;
	snprintf(moon_top_text_str[MOON_NEXT_NEW], 14, moon_mode_names[1], "New");
	moon_top_text_str[MOON_NEXT_NEW][14] = '\0';
	
	time_t tt = get_next_new_moon_date();
	format_and_set_moon_date(tt);

	
	//time_t tt = get_next_new_moon_date();
	//struct tm * tmNewMoon = localtime(&tt);
	//APP_LOG("new moon %d",tmNewMoon->tm_year);
}

void set_full_moon_data() {
	moon_icons[MOON_NEXT_NEW] = FULL_MOON;
	snprintf(moon_top_text_str[MOON_NEXT_NEW], 15, moon_mode_names[1], "Full");
	moon_top_text_str[MOON_NEXT_NEW][15] = '\0';
	
	time_t tt = get_next_full_moon_date();
	format_and_set_moon_date(tt);
	
	
	//time_t tt = get_next_new_moon_date();
	//struct tm * tmNewMoon = localtime(&tt);
	//APP_LOG("new moon %d",tmNewMoon->tm_year);
}


static int get_humidity_img_index(int humPct) {
	int idx = 0;

	if (humPct < 13) {
		idx = HUM_0;
	}
	else if (humPct < 38) {
		idx = HUM_25;
	}
	else if (humPct < 63) {
		idx = HUM_50;
	}
	else if (humPct < 88) {
		idx = HUM_75;
	}
	else {
		idx = HUM_100;
	}

	last_humidity_img_set = idx;
	return idx;
}

void setStatusImage() {
	gbitmap_destroy(current_status_image);
	if (color_mode_normal == true) {
		current_status_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS);
	}
	else {
		current_status_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_INV);
	}

	bitmap_layer_set_bitmap(status_image, current_status_image);
}

void setWindImage() {
	gbitmap_destroy(current_wind_image);
	if (color_mode_normal == true) {
		current_wind_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WIND_DIR);
	}
	else {
		current_wind_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WIND_DIR_INV);
	}

	bitmap_layer_set_bitmap(wind_image, current_wind_image);
}

static void activate_response_mode() {
	prev_active_layer = active_layer;
	response_mode_active = true;
	data_mode = MESSAGES_APP;
	sendCommandInt(SM_SCREEN_ENTER_KEY, MESSAGES_APP);
}



//======================================================================================================
// RECEIVE FUNCTIONS
//======================================================================================================
static void rcv(DictionaryIterator *received, void *context) {
	// Got a message callback

	Tuple *t;
	int *val;
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "RCV");

	t = dict_find(received, SM_WEATHER_DAY1_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_WEATHER_DAY1_KEY VALID");
		memcpy(weather_temp_or_day_str[WEATHER_DAY1], t->value->cstring, strlen(t->value->cstring));
		weather_temp_or_day_str[WEATHER_DAY1][strlen(t->value->cstring)] = '\0';

		// copy the hi lo
		p_weather_hi_lo_str[WEATHER_DAY1] = (char*)&weather_temp_or_day_str[WEATHER_DAY1][6];
		p_weather_hi_lo_str[WEATHER_CURRENT] = p_weather_hi_lo_str[WEATHER_DAY1];

		// copy the day
		//weather_temp_or_day_str[WEATHER_DAY1][4] = '\0';
		memcpy(weather_temp_or_day_str[WEATHER_DAY1], "Today", 5);
		weather_temp_or_day_str[WEATHER_DAY1][5] = '\0';
	}

	t = dict_find(received, SM_WEATHER_DAY2_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_WEATHER_DAY2_KEY VALID");
		memcpy(weather_temp_or_day_str[WEATHER_DAY2], t->value->cstring, strlen(t->value->cstring));
		weather_temp_or_day_str[WEATHER_DAY2][strlen(t->value->cstring)] = '\0';

		// copy the hi lo
		p_weather_hi_lo_str[WEATHER_DAY2] = (char*)&weather_temp_or_day_str[WEATHER_DAY2][6];

		// copy the day
		weather_temp_or_day_str[WEATHER_DAY2][4] = '\0';
	}

	t = dict_find(received, SM_WEATHER_DAY3_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_WEATHER_DAY3_KEY VALID");
		memcpy(weather_temp_or_day_str[WEATHER_DAY3], t->value->cstring, strlen(t->value->cstring));
		weather_temp_or_day_str[WEATHER_DAY3][strlen(t->value->cstring)] = '\0';

		// copy the hi lo
		p_weather_hi_lo_str[WEATHER_DAY3] = (char*)&weather_temp_or_day_str[WEATHER_DAY3][6];

		// copy the day
		weather_temp_or_day_str[WEATHER_DAY3][4] = '\0';
	}

	t = dict_find(received, SM_WEATHER_ICON_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_WEATHER_ICON_KEY VALID");
		int wImg = t->value->uint8;
		Tuple *w = dict_find(received, SM_WEATHER_COND_KEY);
		if (w != NULL) {
			weather_icons[WEATHER_CURRENT] = get_weather_image_index(wImg, WEATHER_CURRENT);
			setWeatherImage(weather_icons[WEATHER_CURRENT]);
		}
	}

	t = dict_find(received, SM_WEATHER_ICON1_KEY);
	if (t != NULL) {
		weather_icons[WEATHER_DAY1] = get_weather_image_index(t->value->uint8, WEATHER_DAY1);
		memcpy(weather_cond_str[WEATHER_DAY1], weather_conditions[t->value->uint8], strlen(weather_conditions[t->value->uint8]));
		weather_cond_str[WEATHER_DAY1][strlen(weather_conditions[t->value->uint8])] = '\0';
	}

	t = dict_find(received, SM_WEATHER_ICON2_KEY);
	if (t != NULL) {
		weather_icons[WEATHER_DAY2] = t->value->uint8;
		memcpy(weather_cond_str[WEATHER_DAY2], weather_conditions[t->value->uint8], strlen(weather_conditions[t->value->uint8]));
		weather_cond_str[WEATHER_DAY2][strlen(weather_conditions[t->value->uint8])] = '\0';
	}

	t = dict_find(received, SM_WEATHER_ICON3_KEY);
	if (t != NULL) {
		weather_icons[WEATHER_DAY3] = t->value->uint8;
		memcpy(weather_cond_str[WEATHER_DAY3], weather_conditions[t->value->uint8], strlen(weather_conditions[t->value->uint8]));
		weather_cond_str[WEATHER_DAY3][strlen(weather_conditions[t->value->uint8])] = '\0';
	}

	t = dict_find(received, SM_WEATHER_COND_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_WEATHER_COND_KEY VALID");
		int len = strlen(t->value->cstring);
		if (len > WEATHER_COND_STRING_LENGTH-2) {
			len = WEATHER_COND_STRING_LENGTH-2;
		}
		
		memcpy(weather_cond_str[WEATHER_CURRENT], t->value->cstring, len);
		weather_cond_str[WEATHER_CURRENT][len] = '\0';
		if (!isDay() && strcmp( weather_cond_str[WEATHER_CURRENT], "Sunny") == 0) {
			text_layer_set_text(text_weather_cond_layer, weather_conditions[0]); 
		} else {
			text_layer_set_text(text_weather_cond_layer, weather_cond_str[WEATHER_CURRENT]);	
		}
		
		text_layer_set_text(text_weather_hi_lo_label_layer, p_weather_hi_lo_str[WEATHER_CURRENT]);
	}

	t = dict_find(received, SM_WEATHER_TEMP_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_WEATHER_TEMP_KEY VALID");
		memcpy(weather_temp_or_day_str[WEATHER_CURRENT], t->value->cstring, strlen(t->value->cstring));
		weather_temp_or_day_str[WEATHER_CURRENT][strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_weather_temp_layer, weather_temp_or_day_str[WEATHER_CURRENT]);
	}

	t = dict_find(received, SM_WEATHER_HUMID_KEY);
	if (t != NULL) {
		int len = strlen(t->value->cstring);
		if (len > HUMIDITY_STRING_LENGTH-2) {
			len = HUMIDITY_STRING_LENGTH-2;
		}
		
		memcpy(weather_humidity_str, t->value->cstring, strlen(t->value->cstring));
		weather_humidity_str[strlen(t->value->cstring)] = '\0';

		bool colonFound = false;
		int newStrIdx = 0;
		for (unsigned int i = 0; i < strlen(weather_humidity_str); i++) {
			if (!colonFound && weather_humidity_str[i] == ':') {
				colonFound = true;
				i += 2; // get rid of the space after the colon
			}

			if (colonFound) {
				weather_humidity_str[newStrIdx] = weather_humidity_str[i];
				++newStrIdx;
			}
		}
		weather_humidity_str[newStrIdx] = '\0';
		text_layer_set_text(text_weather_humidity_layer, weather_humidity_str);

		// Set the proper icon
		if (strlen(weather_humidity_str) > 0 && weather_humidity_str[strlen(weather_humidity_str) - 1] == '%') {
			int humPct = atoi(weather_humidity_str);
			int humIdx = get_humidity_img_index(humPct);
			setHumidityImage(humIdx);
		}
		else {
			setHumidityImage(HUM_50);
		}
	}

	t = dict_find(received, SM_WEATHER_WIND_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_WEATHER_WIND_KEY VALID");
		int len = strlen(t->value->cstring);
		if (len > WIND_STRING_LENGTH-2) {
			len = WIND_STRING_LENGTH-2;
		}
		
		memcpy(weather_wind_str, t->value->cstring, strlen(t->value->cstring));
		weather_wind_str[strlen(t->value->cstring)] = '\0';

		bool colonFound = false;
		int newStrIdx = 0;
		for (unsigned int i = 0; i < strlen(weather_wind_str); i++) {
			if (!colonFound && weather_wind_str[i] == ':') {
				colonFound = true;
				i += 2; // get rid of the space after the colon
			}

			if (colonFound) {
				weather_wind_str[newStrIdx] = weather_wind_str[i];
				++newStrIdx;
			}
		}
		weather_wind_str[newStrIdx] = '\0';
		text_layer_set_text(text_weather_wind_layer, weather_wind_str);
		
		// Switch to calendar mode... sometimes, this may not respond, 
		// so always make sure we get back into status mode
		data_mode = CALENDAR_APP;
		sendCommandInt(SM_SCREEN_ENTER_KEY, CALENDAR_APP);
	}
	
	
	//t = dict_find(received, SM_MESSAGES_UPDATE_KEY);
	//if (t != NULL) {
	//	APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_MESSAGES_UPDATE_KEY VALID");
	//}
	
	t = dict_find(received, SM_CALL_SMS_UPDATE_KEY);
	if (t != NULL) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_CALL_SMS_UPDATE_KEY VALID");
	}
	
	//t = dict_find(received, SM_CALL_SMS_KEY);
	//if (t != NULL) {
	//	APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_CALL_SMS_KEY VALID");
//	}
	
	

	t = dict_find(received, SM_CALENDAR_UPDATE_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "CAL_KEY VALID");
		// Maybe we need this?
		//if (wait_for_msg_app_mode > 0) {
		//	sendCommandInt(SM_CALL_SMS_CMD_KEY, wait_for_msg_app_mode);
		//	wait_for_msg_app_mode = 0;
		//	return;
		//}
		
		uint8_t number_entries = t->value->data[0];
		uint8_t position = 0;
		uint8_t title_subtitle = 0;
		uint8_t len = 0;
		uint8_t copylen = 0;
		
		uint8_t iterCnt = NUM_APPT_MODES;
		int j = 1;
		
		if (data_mode == MESSAGES_APP)
			iterCnt = 1;

		while (j < t->length) {
			position = t->value->data[j++];
			title_subtitle = t->value->data[j++];
			len = t->value->data[j++];
			copylen = len;

			if (position < iterCnt) {
				if (data_mode == MESSAGES_APP) {
					if (title_subtitle == 0) {
						if (copylen > MUSIC_TEXT_STRING_LENGTH-2) {
							copylen = MUSIC_TEXT_STRING_LENGTH-2;
						}
						memcpy(response_mode_bottom_str,&t->value->data[j], copylen);
						response_mode_bottom_str[copylen] = '\0';
						text_layer_set_text(music_artist_layer, response_mode_bottom_str);
					} else {
						if (copylen > MUSIC_TEXT_STRING_LENGTH-2) {
							copylen = MUSIC_TEXT_STRING_LENGTH-2;
						}
						memcpy(response_mode_top_str, &t->value->data[j], copylen);
						response_mode_top_str[copylen] = '\0';
						text_layer_set_text(music_song_layer, response_mode_top_str);
					}
				} else {
					if (title_subtitle == 0) {
						if (copylen > CAL_TEXT_STRING_LENGTH-2) {
							copylen = CAL_TEXT_STRING_LENGTH-2;
						}
						memcpy(calendar_text_str[position], &t->value->data[j], copylen);
						calendar_text_str[position][copylen] = '\0';
					} else {
						if (copylen > DATE_STRING_LENGTH-2) {
							copylen = DATE_STRING_LENGTH-2;
						}
						memcpy(calendar_date_str[position], &t->value->data[j], copylen);
						calendar_date_str[position][copylen] = '\0';
						
						time_t tt = time(NULL);
						struct tm * tm_now = localtime(&tt);
						
						// US Dates MM/DD
						char curDates[1][7];
						snprintf(curDates[0], 6, "%02d/%02d", tm_now->tm_mon+1, tm_now->tm_mday);
						
						// DD/MM format
						//snprintf(curDates[0], 6, "%02d/%02d", tm_now->tm_mday, tm_now->tm_mon+1);
																		
						// DD.MM. format
						//snprintf(curDates[0], 7, "%02d.%02d.", tm_now->tm_mday, tm_now->tm_mon+1);
						
						int strLenDt = 5;
						int idx = 0;
						if (strcmp(curDates[0], calendar_date_str[position]) == 0) {
							strLenDt = 5;
							idx = 0;
						}						
						calendar_date_str[position][strLenDt] = '\0';
						
						if (strcmp(curDates[idx], calendar_date_str[position]) == 0) {
							memmove(&calendar_date_str[position][0], &calendar_date_str[position][strLenDt+1], copylen-(strLenDt+1));
							calendar_date_str[position][copylen-(strLenDt+1)] = '\0';
						} else {
							calendar_date_str[position][strLenDt] = ' ';	
						} 
					}
				}
			}
			j += len;
		}
		
		// Copy default vals in if there are not enough appts to fill
		if (data_mode != MESSAGES_APP) {
			/*
			for (int k=(position+1); k < NUM_APPT_MODES; k++) {
				memcpy(calendar_date_str[k], default_cal_names[0], strlen(default_cal_names[0]));
				calendar_date_str[k][strlen(default_cal_names[0])] = '\0';
				memcpy(calendar_text_str[k], default_cal_names[1], strlen(default_cal_names[1]));
				calendar_text_str[k][strlen(default_cal_names[1])] = '\0';
			}
			*/
			actual_num_appt_modes = position < NUM_APPT_MODES ? position : NUM_APPT_MODES;
			int oldAppt = active_appt_mode_index;
			int oldStatusAppt = active_appt_status_mode_index;
			active_appt_mode_index = -1;
			active_appt_status_mode_index = -1;
			handle_calendar_mode_click(oldAppt);
			handle_calendar_status_mode_click(oldStatusAppt);
		
			// Reset into smartwatch mode to make sure we get auto updates
			//app_timer_cancel(statusModeTimer);
			data_mode = STATUS_SCREEN_APP;
			sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
		} else {
			transition_main_layer(MUSIC_LAYER); // temp response layer
			set_info_text_with_timer("Response Mode", response_mode_timeout);
			responseModeTimer = app_timer_register(response_mode_timeout, dismiss_response_mode, NULL);
		}
			
	}

	t = dict_find(received, SM_COUNT_MAIL_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_COUNT_MAIL_KEY VALID");
		int len = strlen(t->value->cstring);
		if (len > BADGE_COUNT_STRING_LENGTH-2) {
			len = BADGE_COUNT_STRING_LENGTH-2;
		}
		
		memcpy(mail_count_str, t->value->cstring, len);
		mail_count_str[len] = '\0';
		text_layer_set_text(text_mail_layer, mail_count_str);
	}

	t = dict_find(received, SM_COUNT_SMS_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_COUNT_SMS_KEY VALID");
		int len = strlen(t->value->cstring);
		if (len > BADGE_COUNT_STRING_LENGTH-2) {
			len = BADGE_COUNT_STRING_LENGTH-2;
		}
		
		memcpy(sms_count_str, t->value->cstring, len);
		sms_count_str[len] = '\0';
		text_layer_set_text(text_sms_layer, sms_count_str);
		
		int cnt = atoi(sms_count_str);
		if (cnt > prev_sms_count && prev_sms_count != -1) {
			//activate_response_mode();
		}
		prev_sms_count = cnt;
	}

	t = dict_find(received, SM_COUNT_PHONE_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_COUNT_PHONE_KEY VALID");
		int len = strlen(t->value->cstring);
		if (len > BADGE_COUNT_STRING_LENGTH-2) {
			len = BADGE_COUNT_STRING_LENGTH-2;
		}
		memcpy(phone_count_str, t->value->cstring, len);
		phone_count_str[len] = '\0';
		text_layer_set_text(text_phone_layer, phone_count_str);
		
		// Not sure how to get and display last caller yet
		/*
		int cnt = atoi(phone_count_str);
		if (cnt > prev_phone_count && prev_phone_count != -1) {
			activate_response_mode();
		}
		prev_phone_count = cnt;
		*/
	}

	t = dict_find(received, SM_COUNT_BATTERY_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_COUNT_BATTERY_KEY VALID");
		batteryPercent = t->value->uint8;
		layer_mark_dirty(battery_layer);

		snprintf(battery_pct_str, sizeof(battery_pct_str), "%d", batteryPercent);
		text_layer_set_text(text_battery_layer, battery_pct_str);
	}

	t = dict_find(received, SM_STATUS_MUS_ARTIST_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_STATUS_MUS_ARTIST_KEY VALID");
		int len = strlen(t->value->cstring);
		if (len > MUSIC_TEXT_STRING_LENGTH-2) {
			len = MUSIC_TEXT_STRING_LENGTH-2;
		}
		
		memcpy(music_artist_str, t->value->cstring, len);
		music_artist_str[len] = '\0';
		
		if (!response_mode_active)
			text_layer_set_text(music_artist_layer, music_artist_str);
	}

	t = dict_find(received, SM_STATUS_MUS_TITLE_KEY);
	if (t != NULL) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "SM_STATUS_MUS_TITLE_KEY VALID");
		int len = strlen(t->value->cstring);
		if (len > MUSIC_TEXT_STRING_LENGTH-2) {
			len = MUSIC_TEXT_STRING_LENGTH-2;
		}
		memcpy(music_title_str, t->value->cstring, len);
		music_title_str[len] = '\0';
		
		if (!response_mode_active)
			text_layer_set_text(music_song_layer, music_title_str);
	}
}


//======================================================================================================
// END RECEIVE FUNCTIONS
//======================================================================================================

static void toggle_themes()
{
	GColor textColor;
	if (color_mode_normal == 0)	{
		color_mode_normal = 1;
		textColor = GColorWhite;
	}
	else {
		color_mode_normal = 0;
		textColor = GColorBlack;
	}

	persist_write_bool(FS_CONFIG_KEY_THEME, color_mode_normal);

	gbitmap_destroy(bg_image);

	if (color_mode_normal == 1)
		bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
	else
		bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_INV);

	bitmap_layer_set_bitmap(background_image, bg_image);

	setStatusImage();

	text_layer_set_text_color(text_weather_cond_layer, textColor);
	text_layer_set_text_color(text_weather_temp_layer, textColor);
	text_layer_set_text_color(text_weather_humidity_layer, textColor);
	text_layer_set_text_color(text_weather_wind_layer, textColor);
	text_layer_set_text_color(text_weather_hi_lo_label_layer, textColor);
	text_layer_set_text_color(text_date_layer, textColor);
	text_layer_set_text_color(text_time_layer, textColor);
	text_layer_set_text_color(moon_text_layer, textColor);
	text_layer_set_text_color(moon_phase_layer, textColor);
	text_layer_set_text_color(calendar_date_layer1, textColor);
	text_layer_set_text_color(calendar_text_layer1, textColor);
	text_layer_set_text_color(calendar_date_layer2, textColor);
	text_layer_set_text_color(calendar_text_layer2, textColor);
	text_layer_set_text_color(music_artist_layer, textColor);
	text_layer_set_text_color(music_song_layer, textColor);
	text_layer_set_text_color(text_mail_layer, textColor);
	text_layer_set_text_color(text_sms_layer, textColor);
	text_layer_set_text_color(text_phone_layer, textColor);
	text_layer_set_text_color(text_battery_layer, textColor);
	text_layer_set_text_color(text_weather_humidity_layer, textColor);
	text_layer_set_text_color(text_weather_humidity_label_layer, textColor);
	text_layer_set_text_color(text_weather_wind_layer, textColor);
	text_layer_set_text_color(text_weather_wind_label_layer, textColor);

	setWeatherImage(last_weather_img_set);
	setHumidityImage(last_humidity_img_set);
	setWindImage();
	set_moon_data();

	Layer *window_layer = window_get_root_layer(window);
	layer_mark_dirty(window_layer);
}


//======================================================================================================
// BUTTON HANDLERS
//======================================================================================================


static void transition_main_layer(int view) {
	if (view == active_layer) {
		return;
	}

	if (ani_in != NULL) {
		property_animation_destroy(ani_in);
		property_animation_destroy(ani_out);	
	}
		
	ani_out = property_animation_create_layer_frame(animated_layer[active_layer], &GRect(0, 77, 144, 45), &GRect(-144, 77, 144, 45));
	animation_schedule((Animation*)ani_out);
	
	if (view >= 0) {
		active_layer = view;
	}
	else {
		active_layer = (active_layer + 1) % (NUM_LAYERS);
	}

	ani_in = property_animation_create_layer_frame(animated_layer[active_layer], &GRect(144, 77, 144, 45), &GRect(0, 77, 144, 45));
	animation_schedule((Animation*)ani_in);
}



static void transition_status_layer(int view) {
	if (view == active_status_layer) {
		return;
	}

	if (ani_in_status != NULL) {
		property_animation_destroy(ani_in_status);
		property_animation_destroy(ani_out_status);	
	}
	
	
	ani_out_status = property_animation_create_layer_frame(status_layer[active_status_layer], &GRect(0, 125, 144, 53), &GRect(-144, 125, 144, 53));
	animation_schedule((Animation*)ani_out_status);
	
	if (view >= 0) {
		active_status_layer = view;
	}
	else {
		active_status_layer = (active_status_layer + 1) % (NUM_STATUS_LAYERS);
	}

	ani_in_status = property_animation_create_layer_frame(status_layer[active_status_layer], &GRect(144, 125, 144, 48), &GRect(0, 125, 144, 48));
	animation_schedule((Animation*)ani_in_status);
}

static void handle_weather_mode_click(int mode){
	if (mode == active_weather_mode_index) {
		return;
	}
	else if (mode >= 0) {
		active_weather_mode_index = mode;
	}
	else {
		active_weather_mode_index = (active_weather_mode_index + 1) % (NUM_WEATHER_MODES);
	}

	if (active_weather_mode_index == WEATHER_CURRENT) {
		text_layer_set_font(text_weather_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	}
	else {
		text_layer_set_font(text_weather_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	}

	text_layer_set_text(text_weather_cond_layer, weather_cond_str[active_weather_mode_index]);
	text_layer_set_text(text_weather_hi_lo_label_layer, p_weather_hi_lo_str[active_weather_mode_index]);
	text_layer_set_text(text_weather_temp_layer, weather_temp_or_day_str[active_weather_mode_index]);
	setWeatherImage(weather_icons[active_weather_mode_index]);
}

static void handle_moon_mode_click(int mode){
	if (mode == active_moon_mode_index) {
		return;
	} else if (mode >= 0) {
		active_moon_mode_index = mode;
	} else {
		active_moon_mode_index = (active_moon_mode_index + 1) % (NUM_MOON_MODES);
	}

	if (active_moon_mode_index == MOON_CURRENT) {
		set_moon_data();
	} else if (active_moon_mode_index == MOON_NEXT_NEW) {
		set_new_moon_data();
	} else if (active_moon_mode_index == MOON_NEXT_FULL) {
		set_full_moon_data();
	}
	
}

static void handle_calendar_mode_click(int mode) {
	if (mode == active_appt_mode_index) {
		return;
	} else if (mode >= 0) {
		active_appt_mode_index = mode;
	} else {
		active_appt_mode_index = (active_appt_mode_index + 1) % (actual_num_appt_modes);
	}
	
	text_layer_set_text(calendar_text_layer1, calendar_text_str[active_appt_mode_index]);
	text_layer_set_text(calendar_date_layer1, calendar_date_str[active_appt_mode_index]);
	//layer_mark_dirty(animated_layer[CALENDAR_LAYER]);
}

static void handle_calendar_status_mode_click(int mode) {
	if (active_appt_status_mode_index == mode) {
		return;
	} else if (mode >= 0) {
		active_appt_status_mode_index = mode;
	} else {
		active_appt_status_mode_index = (active_appt_status_mode_index + 1) % (actual_num_appt_modes);
	}
	
	text_layer_set_text(calendar_text_layer2, calendar_text_str[active_appt_status_mode_index]);
	text_layer_set_text(calendar_date_layer2, calendar_date_str[active_appt_status_mode_index]);
}

static void set_info_text_with_timer(char * txt, int time) {
	text_layer_set_text(text_date_layer, txt);
	set_timer_for_date_recovery(time);
}

static void set_info_text(int mode) {
	if (mode == 0) {
		if (active_layer == WEATHER_LAYER) {
			set_info_text_with_timer(weather_mode_names[active_weather_mode_index], date_switchback_long * 2);
		} else if (active_layer == MOON_LAYER) {
			if (active_moon_mode_index == 0) {
				set_info_text_with_timer(moon_mode_names[0], date_switchback_long);	
			} else {
				set_info_text_with_timer(moon_top_text_str[MOON_NEXT_NEW], date_switchback_long);	
			}
			
		} else if (active_layer == CALENDAR_LAYER) {
			set_info_text_with_timer(appt_mode_names[active_appt_mode_index], date_switchback_long);
		} else {
			set_info_text_with_timer(layer_names[active_layer], date_switchback_long);
		}	
	} 
	else {
		if (active_status_layer == CALENDAR_STATUS_LAYER) {
			set_info_text_with_timer(appt_mode_names[active_appt_status_mode_index], date_switchback_long);
		} else {
			set_info_text_with_timer(status_layer_names[active_status_layer], date_switchback_long);
		}
	}
	
	//set_timer_for_date_recovery(date_switchback_long);
}

void dismiss_response_mode() {
	app_timer_cancel(responseModeTimer);
	response_mode_active = false;
	transition_main_layer(prev_active_layer);
	get_date_back(NULL);
	// Restore the real music layer
	text_layer_set_text(music_artist_layer, music_artist_str);
	text_layer_set_text(music_song_layer, music_title_str);
	
	data_mode = STATUS_SCREEN_APP;
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
}

void reset_views_and_modes()
{
	// Return views to default
	transition_status_layer(STATUS_LAYER);
	transition_main_layer(WEATHER_LAYER);
	
	// Return views to default modes
	handle_weather_mode_click(WEATHER_CURRENT);
	handle_calendar_mode_click(APPT_1);
	handle_moon_mode_click(MOON_CURRENT);
	handle_calendar_status_mode_click(APPT_1);
	
	set_info_text(MIDDLE_LAYERS);	
}

// ===== UP ======
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) {
		// Sends canned txt 1
		//wait_for_msg_app_mode = 1;
		//sendCommandInt(SM_SCREEN_ENTER_KEY, MESSAGES_APP);
		sendCommandInt(SM_CALL_SMS_CMD_KEY, 1);
		dismiss_response_mode();
		set_info_text_with_timer("Sent SMS 1", date_switchback_long);
	} else if (active_layer == MUSIC_LAYER) {
		set_info_text_with_timer("Volume up", date_switchback_short);
		sendCommand(SM_VOLUME_UP_KEY);
	} else if (active_layer == MOON_LAYER) {
		handle_moon_mode_click(NEXT_ITEM);
		set_info_text(MIDDLE_LAYERS);
	} else if (active_layer == WEATHER_LAYER) {
		handle_weather_mode_click(NEXT_ITEM);
		set_info_text(MIDDLE_LAYERS);
	} else if (active_layer == CALENDAR_LAYER) {
		handle_calendar_mode_click(NEXT_ITEM);
		set_info_text(MIDDLE_LAYERS);
	}
}

static void up_double_click_handler (ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) return;
	
	if (active_status_layer == CALENDAR_STATUS_LAYER) {
		handle_calendar_status_mode_click(NEXT_ITEM);	
		set_info_text(BOTTOM_LAYERS);
	}
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) return;
	
	if (active_layer == MUSIC_LAYER) {
		set_info_text_with_timer("Previous track", date_switchback_short);
		sendCommand(SM_PREVIOUS_TRACK_KEY);
	} else {
		text_layer_set_text(text_weather_cond_layer, updating_str);
		text_layer_set_text(text_weather_wind_layer, updating_str);
		text_layer_set_text(calendar_date_layer1, updating_str);
		text_layer_set_text(calendar_date_layer2, updating_str);
		set_info_text_with_timer(updating_str, date_switchback_short);
		set_timer_for_date_recovery(date_switchback_short);
		set_moon_data();
		data_update_and_refresh();
	}
}

// ===== SELECT ======
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) {
		dismiss_response_mode();
		return;
	}
	
	transition_main_layer(NEXT_ITEM);
	set_info_text(MIDDLE_LAYERS);
}
static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) return;
	
	if (active_layer == MUSIC_LAYER) {
		set_info_text_with_timer("Play/Pause", date_switchback_short);
		sendCommand(SM_PLAYPAUSE_KEY);
	} else {
		set_info_text_with_timer("Invoke Siri", date_switchback_short);
		//text_layer_set_text(text_date_layer, "Invoke Siri");
		sendCommand(SM_OPEN_SIRI_KEY);
		//set_timer_for_date_recovery(date_switchback_short);
	}
}


static void select_double_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) return;
	
	if (active_layer == CALENDAR_LAYER && active_status_layer == CALENDAR_STATUS_LAYER) {
		reset_views_and_modes();			
	} else {
		set_info_text_with_timer("Appointments", date_switchback_long);

		handle_calendar_status_mode_click(APPT_2);
		handle_calendar_mode_click(APPT_1);
		transition_status_layer(CALENDAR_STATUS_LAYER);
		transition_main_layer(CALENDAR_LAYER);
	}
}

// ===== DOWN ======

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) {
		// Sends canned txt 2
		//wait_for_msg_app_mode = 2;
		//sendCommandInt(SM_SCREEN_ENTER_KEY, MESSAGES_APP);
		sendCommandInt(SM_CALL_SMS_CMD_KEY, 2);
		dismiss_response_mode();
		set_info_text_with_timer("Sent SMS 2", date_switchback_long);
	} else if (active_layer == MUSIC_LAYER) {
		set_info_text_with_timer("Volume down", date_switchback_short);
		sendCommand(SM_VOLUME_DOWN_KEY);
	} else {
		transition_status_layer(NEXT_ITEM);
		set_info_text(BOTTOM_LAYERS);
	}
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) return;
	
	if (response_mode_active) {
		// Sends canned txt 2
		sendCommandInt(SM_CALL_SMS_CMD_KEY, 2);	
		dismiss_response_mode();
		set_info_text_with_timer("SMS Response 2", date_switchback_long);
	} else if (active_layer == MUSIC_LAYER) {
		set_info_text_with_timer("Next track", date_switchback_short);
		sendCommand(SM_NEXT_TRACK_KEY);
	} else {
		toggle_themes();
	}
}

static void down_double_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (response_mode_active) return;
	
	transition_main_layer(MUSIC_LAYER);
	set_info_text(MIDDLE_LAYERS);
}


static void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
	window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL /* No handler on button release */);
	window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 0, 0, true, select_double_click_handler);

	window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
	window_long_click_subscribe(BUTTON_ID_UP, 500, up_long_click_handler, NULL);
	window_multi_click_subscribe(BUTTON_ID_UP, 2, 0, 0, true, up_double_click_handler);

	window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
	window_long_click_subscribe(BUTTON_ID_DOWN, 500, down_long_click_handler, NULL);
	window_multi_click_subscribe(BUTTON_ID_DOWN, 2, 0, 0, true, down_double_click_handler);
}

//======================================================================================================
// END BUTTON HANDLERS
//======================================================================================================


//======================================================================================================
// CALLBACKS
//======================================================================================================
void reset() {
	text_layer_set_text(text_weather_cond_layer, updating_str);
}

void setCalendarIcons(GContext* ctx, const char *iconNum, GPoint pt, GRect txtRect) {
	GColor fore;
	GColor fill;
	if (color_mode_normal == 1){
		fore = GColorBlack;
		fill = GColorWhite;
	}
	else {
		fore = GColorWhite;
		fill = GColorBlack;
	}

	if (iconNum[0] >= '4') {
		txtRect.origin.x--;
	}
	
	graphics_context_set_fill_color(ctx, fill);
	graphics_fill_circle(ctx, pt, 8);
	graphics_context_set_text_color(ctx, fore);
	graphics_draw_text(ctx, iconNum, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), txtRect, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

void cal_update_callback(Layer *me, GContext* ctx) {
	static char badgeNum[2];
	badgeNum[0] = (char)(active_appt_mode_index + 49);
	badgeNum[1] = '\0';
	setCalendarIcons(ctx, badgeNum, GPoint(14, 12), GRect(11, 0, 20, 30));
}

void cal_status_update_callback(Layer *me, GContext* ctx) {
	static char badgeNum2[2];
	badgeNum2[0] = (char)(active_appt_status_mode_index + 49);
	badgeNum2[1] = '\0';
	setCalendarIcons(ctx, badgeNum2, GPoint(14, 11),GRect(11, -1, 20, 30));
}

void battery_layer_update_callback(Layer *me, GContext* ctx) {
	GColor stroke;
	GColor fill;
	if (color_mode_normal == 1){
		stroke = GColorBlack;
		fill = GColorWhite;
	}
	else {
		stroke = GColorWhite;
		fill = GColorBlack;
	}

	graphics_context_set_stroke_color(ctx, stroke);
	graphics_context_set_fill_color(ctx, fill);

	graphics_fill_rect(ctx, GRect(15 - (int)((batteryPercent / 100.0)*15.0), 0, 15, 7), 0, GCornerNone);
}

void pebble_battery_layer_update_callback(Layer *me, GContext* ctx) {
	GColor stroke;
	GColor fill;
	if (color_mode_normal == 1){
		stroke = GColorBlack;
		fill = GColorWhite;
	}
	else {
		stroke = GColorWhite;
		fill = GColorBlack;
	}

	graphics_context_set_stroke_color(ctx, stroke);
	graphics_context_set_fill_color(ctx, fill);

	graphics_fill_rect(ctx, GRect(15 - (int)((batteryPblPercent / 100.0)*15.0), 0, 15, 7), 0, GCornerNone);
}




static void window_load(Window *window) {
	
}

static void window_unload(Window *window) {

}

static void window_appear(Window *window) {
	//sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	data_update_and_refresh();
}


static void window_disappear(Window *window) {
	//sendCommandInt(SM_SCREEN_EXIT_KEY, STATUS_SCREEN_APP);
	data_update_and_refresh();
}

void reconnect(void *data) {
	reset();
	//sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	data_update_and_refresh();
}

void bluetoothChanged(bool connected) {
	if (connected) {
		app_timer_register(5000, reconnect, NULL);
	}
	else {
		setWeatherImage(NUM_WEATHER_IMAGES - 1);
		vibes_double_pulse();
	}
}


void batteryChanged(BatteryChargeState batt) {
	batteryPblPercent = batt.charge_percent;
	layer_mark_dirty(battery_layer);
}

//======================================================================================================
// END CALLBACKS
//======================================================================================================

//======================================================================================================
// INIT / DEINIT
//======================================================================================================
static void text_layer_setup(Layer * parentLayer, TextLayer * layer, GTextAlignment tAlign, GColor textColor, const char * fontkey, const char * txt) {
	text_layer_set_text_alignment(layer, tAlign);
	text_layer_set_text_color(layer, textColor);
	text_layer_set_background_color(layer, GColorClear);
	text_layer_set_font(layer, fonts_get_system_font(fontkey));
	layer_add_child(parentLayer, text_layer_get_layer(layer));
	text_layer_set_text(layer, txt);
}

static void add_and_set_bitmap_layer(Layer * parentLayer, BitmapLayer * layer, GBitmap * img) {
	layer_add_child(parentLayer, bitmap_layer_get_layer(layer));
	bitmap_layer_set_bitmap(layer, img);
}

static void init(void) {
	window = window_create();
	window_set_fullscreen(window, true);

	window_set_click_config_provider(window, click_config_provider);
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
			.unload = window_unload,
			.appear = window_appear,
			.disappear = window_disappear
	});

	color_mode_normal = persist_exists(FS_CONFIG_KEY_THEME) ? persist_read_bool(FS_CONFIG_KEY_THEME) : true;

	const bool animated = true;
	window_stack_push(window, animated);

	GColor textColor;

	if (color_mode_normal == false) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "Theme is white");
		textColor = GColorBlack;
	}
	else {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "Theme is black");
		textColor = GColorWhite;
	}

	if (color_mode_normal == true)
		bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
	else
		bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_INV);


	Layer *window_layer = window_get_root_layer(window);

	//init background image
	GRect bg_bounds = layer_get_frame(window_layer);

	background_image = bitmap_layer_create(bg_bounds);
	layer_add_child(window_layer, bitmap_layer_get_layer(background_image));
	bitmap_layer_set_bitmap(background_image, bg_image);
	
	//---------------------------------------------------------------------------
	// TIME and DATE layers
	//---------------------------------------------------------------------------
	text_date_layer = text_layer_create(bg_bounds);
	text_layer_setup(window_layer, text_date_layer, GTextAlignmentCenter, textColor, FONT_KEY_ROBOTO_CONDENSED_21, "");
	layer_set_frame(text_layer_get_layer(text_date_layer), GRect(0, 44, 144, 30));
	
	text_time_layer = text_layer_create(bg_bounds);
	text_layer_setup(window_layer, text_time_layer, GTextAlignmentCenter, textColor, FONT_KEY_ROBOTO_BOLD_SUBSET_49, "");
	layer_set_frame(text_layer_get_layer(text_time_layer), GRect(0, -6, 144, 50));
	
	
	//---------------------------------------------------------------------------
	// Weather Layer
	//---------------------------------------------------------------------------
	animated_layer[WEATHER_LAYER] = layer_create(GRect(0, 77, 144, 45));
	layer_add_child(window_layer, animated_layer[WEATHER_LAYER]);

	text_weather_hi_lo_label_layer = text_layer_create(GRect(90, 23, 49, 40));
	text_layer_setup(animated_layer[WEATHER_LAYER], text_weather_hi_lo_label_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18_BOLD, "-/-");
	
	text_weather_cond_layer = text_layer_create(GRect(5, 2, 47, 40));
	text_layer_setup(animated_layer[WEATHER_LAYER], text_weather_cond_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18, updating_str);

	text_weather_temp_layer = text_layer_create(GRect(92, -3, 47, 40));
	text_layer_setup(animated_layer[WEATHER_LAYER], text_weather_temp_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_28, "-°");
	
	//---------------------------------------------------------------------------
	// Weather2 Layer (Wind & Humidity)
	//---------------------------------------------------------------------------
	animated_layer[WEATHER_LAYER2] = layer_create(GRect(144, 77, 144, 45));
	layer_add_child(window_layer, animated_layer[WEATHER_LAYER2]);

	text_weather_humidity_label_layer = text_layer_create(GRect(17, 1, 76 - 17, 28));
	text_layer_setup(animated_layer[WEATHER_LAYER2], text_weather_humidity_label_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18_BOLD, "Humidity");
	
	text_weather_humidity_layer = text_layer_create(GRect(17, 18, 76 - 17, 28));
	text_layer_setup(animated_layer[WEATHER_LAYER2], text_weather_humidity_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18, "-");
	
	text_weather_wind_label_layer = text_layer_create(GRect(93, 1, 144 - 91, 28));
	text_layer_setup(animated_layer[WEATHER_LAYER2], text_weather_wind_label_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18_BOLD, "Wind");
	
	text_weather_wind_layer = text_layer_create(GRect(76, 18, 144 - 76, 28));
	text_layer_setup(animated_layer[WEATHER_LAYER2], text_weather_wind_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18, "-");
	
	//---------------------------------------------------------------------------
	// Moon Layer
	//---------------------------------------------------------------------------
	animated_layer[MOON_LAYER] = layer_create(GRect(144, 77, 144, 45));
	layer_add_child(window_layer, animated_layer[MOON_LAYER]);

	moon_text_layer = text_layer_create(GRect(32, 0, 132, 28));
	text_layer_setup(animated_layer[MOON_LAYER], moon_text_layer, GTextAlignmentLeft, textColor, FONT_KEY_GOTHIC_18, "-");
	
	moon_phase_layer = text_layer_create(GRect(32, 15, 132, 28));
	text_layer_setup(animated_layer[MOON_LAYER], moon_phase_layer, GTextAlignmentLeft, textColor, FONT_KEY_GOTHIC_24_BOLD, "-");
	
	//---------------------------------------------------------------------------
	// Calendar Layer
	//---------------------------------------------------------------------------
	animated_layer[CALENDAR_LAYER] = layer_create(GRect(144, 77, 144, 45));
	layer_add_child(window_layer, animated_layer[CALENDAR_LAYER]);
	layer_set_update_proc(animated_layer[CALENDAR_LAYER], cal_update_callback);

	calendar_date_layer1 = text_layer_create(GRect(26, 0, 112, 21));
	text_layer_setup(animated_layer[CALENDAR_LAYER], calendar_date_layer1, GTextAlignmentLeft, textColor, FONT_KEY_GOTHIC_18, default_cal_names[0]);
	
	calendar_text_layer1 = text_layer_create(GRect(6, 15, 132, 28));
	text_layer_setup(animated_layer[CALENDAR_LAYER], calendar_text_layer1, GTextAlignmentLeft, textColor, FONT_KEY_GOTHIC_24_BOLD, default_cal_names[1]);
	
	//---------------------------------------------------------------------------
	// Music Layer
	//---------------------------------------------------------------------------
	animated_layer[MUSIC_LAYER] = layer_create(GRect(144, 77, 144, 45));
	layer_add_child(window_layer, animated_layer[MUSIC_LAYER]);

	music_artist_layer = text_layer_create(GRect(6, 0, 132, 21));
	text_layer_setup(animated_layer[MUSIC_LAYER], music_artist_layer, GTextAlignmentLeft, textColor, FONT_KEY_GOTHIC_18, "Artist");
	
	music_song_layer = text_layer_create(GRect(6, 15, 132, 28));
	text_layer_setup(animated_layer[MUSIC_LAYER], music_song_layer, GTextAlignmentLeft, textColor, FONT_KEY_GOTHIC_24_BOLD, "Title");
	
	//---------------------------------------------------------------------------
	// Status Layer (Phone Data)
	//---------------------------------------------------------------------------
	status_layer[STATUS_LAYER] = layer_create(GRect(0, 125, 144, 53));
	layer_add_child(window_layer, status_layer[STATUS_LAYER]);

	text_mail_layer = text_layer_create(GRect(5, 23, 32, 48));
	text_layer_setup(status_layer[STATUS_LAYER], text_mail_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18_BOLD, "-");
	
	text_sms_layer = text_layer_create(GRect(44, 23, 30, 48));
	text_layer_setup(status_layer[STATUS_LAYER], text_sms_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18_BOLD, "-");
	
	text_phone_layer = text_layer_create(GRect(78, 23, 23, 48));
	text_layer_setup(status_layer[STATUS_LAYER], text_phone_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18_BOLD, "-");
	
	text_battery_layer = text_layer_create(GRect(105, 23, 30, 48));
	text_layer_setup(status_layer[STATUS_LAYER], text_battery_layer, GTextAlignmentCenter, textColor, FONT_KEY_GOTHIC_18_BOLD, "-");
	
	//---------------------------------------------------------------------------
	// Calendar2 Layer (bottom view)
	//---------------------------------------------------------------------------
	status_layer[CALENDAR_STATUS_LAYER] = layer_create(GRect(144, 125, 144, 48));
	layer_add_child(window_layer, status_layer[CALENDAR_STATUS_LAYER]);
	layer_set_update_proc(status_layer[CALENDAR_STATUS_LAYER], cal_status_update_callback);

	calendar_date_layer2 = text_layer_create(GRect(26, -1, 112, 21));
	text_layer_setup(status_layer[CALENDAR_STATUS_LAYER], calendar_date_layer2, GTextAlignmentLeft, textColor, FONT_KEY_GOTHIC_18, default_cal_names[0]);
	
	calendar_text_layer2 = text_layer_create(GRect(6, 13, 132, 28));
	text_layer_setup(status_layer[CALENDAR_STATUS_LAYER], calendar_text_layer2, GTextAlignmentLeft, textColor, FONT_KEY_GOTHIC_24_BOLD, default_cal_names[1]);
	
	if (bluetooth_connection_service_peek()) {
		weather_img = 0;
	}
	else {
		weather_img = NUM_WEATHER_IMAGES - 1;
	}

	// Images
	//---------------------------------------------------------------------------
	// Status image (phone, sms bubble, email, etc...)
	//---------------------------------------------------------------------------
	if (color_mode_normal == 1)
		current_status_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS);
	else
		current_status_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_INV);
	status_image = bitmap_layer_create(GRect(0, 0, 144, 27));
	add_and_set_bitmap_layer(status_layer[STATUS_LAYER], status_image, current_status_image);

	//---------------------------------------------------------------------------
	// Battery layers
	//---------------------------------------------------------------------------
	battery_layer = layer_create(GRect(112, 18, 15, 7));
	layer_set_update_proc(battery_layer, battery_layer_update_callback);
	layer_add_child(status_layer[STATUS_LAYER], battery_layer);

	batteryPercent = 100;
	layer_mark_dirty(battery_layer);

	pebble_battery_layer = layer_create(GRect(112, 5, 15, 7));
	layer_set_update_proc(pebble_battery_layer, pebble_battery_layer_update_callback);
	layer_add_child(status_layer[STATUS_LAYER], pebble_battery_layer);

	BatteryChargeState pbl_batt = battery_state_service_peek();
	batteryPblPercent = pbl_batt.charge_percent;
	layer_mark_dirty(pebble_battery_layer);

	//---------------------------------------------------------------------------
	// Main weather icon
	//---------------------------------------------------------------------------
	if (color_mode_normal == 1)
		current_weather_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN);
	else
		current_weather_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN_INV);
	weather_image = bitmap_layer_create(GRect(52, 2, 40, 40));
	add_and_set_bitmap_layer(animated_layer[WEATHER_LAYER], weather_image, current_weather_image);
	

	//---------------------------------------------------------------------------
	// Humidity icon
	//---------------------------------------------------------------------------
	if (color_mode_normal == 1)
		current_humidity_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HUMIDITY_50);
	else
		current_humidity_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HUMIDITY_50_INV);
	humidity_image = bitmap_layer_create(GRect(1, 5, 16, 36));
	add_and_set_bitmap_layer(animated_layer[WEATHER_LAYER2], humidity_image, current_humidity_image);
	
	//---------------------------------------------------------------------------
	// Wind icon
	//---------------------------------------------------------------------------
	if (color_mode_normal == 1)
		current_wind_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WIND_DIR);
	else
		current_wind_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WIND_DIR_INV);
	wind_image = bitmap_layer_create(GRect(78, 5, 23, 15));
	add_and_set_bitmap_layer(animated_layer[WEATHER_LAYER2], wind_image, current_wind_image);

	//---------------------------------------------------------------------------
	// Moon icon
	//---------------------------------------------------------------------------
	if (color_mode_normal == true)
		current_moon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON_NEW);
	else
		current_moon_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON_NEW_INV);
	moon_image = bitmap_layer_create(GRect(1, 7, 30, 30));
	add_and_set_bitmap_layer(animated_layer[MOON_LAYER], moon_image, current_moon_image);

	
	active_layer = WEATHER_LAYER;
	active_status_layer = STATUS_LAYER;

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

	bluetooth_connection_service_subscribe(bluetoothChanged);
	battery_state_service_subscribe(batteryChanged);
}


static void deinit(void) {
	animation_destroy((Animation*)ani_in);
	animation_destroy((Animation*)ani_out);
	animation_destroy((Animation*)ani_in_status);
	animation_destroy((Animation*)ani_out_status);

	text_layer_destroy(text_weather_cond_layer);
	text_layer_destroy(text_weather_temp_layer);
	text_layer_destroy(text_weather_hi_lo_label_layer);
	text_layer_destroy(text_date_layer);
	text_layer_destroy(text_time_layer);
	text_layer_destroy(text_mail_layer);
	text_layer_destroy(text_sms_layer);
	text_layer_destroy(text_phone_layer);
	text_layer_destroy(text_battery_layer);
	text_layer_destroy(moon_text_layer);
	text_layer_destroy(moon_phase_layer);
	text_layer_destroy(text_weather_humidity_layer);
	text_layer_destroy(text_weather_humidity_label_layer);
	text_layer_destroy(text_weather_wind_layer);
	text_layer_destroy(text_weather_wind_label_layer);
	text_layer_destroy(calendar_date_layer1);
	text_layer_destroy(calendar_text_layer1);
	text_layer_destroy(calendar_date_layer2);
	text_layer_destroy(calendar_text_layer2);
	text_layer_destroy(music_artist_layer);
	text_layer_destroy(music_song_layer);
	text_layer_destroy(text_weather_day2_layer);
	text_layer_destroy(text_weather_day3_layer);
	text_layer_destroy(text_weather_day2_cond_layer);
	text_layer_destroy(text_weather_day3_cond_layer);

	layer_destroy(battery_layer);
	layer_destroy(pebble_battery_layer);
	bitmap_layer_destroy(background_image);
	bitmap_layer_destroy(weather_image);
	bitmap_layer_destroy(moon_image);
	bitmap_layer_destroy(status_image);
	bitmap_layer_destroy(wind_image);
	bitmap_layer_destroy(humidity_image);
	
	for (int i = 0; i<NUM_STATUS_LAYERS; i++) {
		layer_destroy(status_layer[i]);
	}

	for (int i = 0; i<NUM_LAYERS; i++) {
		layer_destroy(animated_layer[i]);
	}

	gbitmap_destroy(current_weather_image);
	gbitmap_destroy(current_moon_image);
	gbitmap_destroy(current_status_image);
	gbitmap_destroy(current_wind_image);
	gbitmap_destroy(current_humidity_image);
	gbitmap_destroy(bg_image);

	tick_timer_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();

	window_stack_remove(window, false);
	window_destroy(window);
}

//======================================================================================================
// END INIT / DEINIT
//======================================================================================================


int main(void) {
	//app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum() );
	app_message_open(app_message_inbox_size_maximum(), 50);
	app_message_register_inbox_received(rcv);

	init();

	app_event_loop();
	app_message_deregister_callbacks();

	deinit();
}
