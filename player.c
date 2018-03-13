#include <gst/gst.h>
#include <glib.h>
#include <stdarg.h>
#define recallMe return TRUE
/*
any_event_listener(bus,msg,data)
called on every signal found on bus
reacts only on EOS and ERROR by stopping the mainloop
@return TRUE so to be called again*/
static gboolean any_event_listener(GstBus *bus,GstMessage *msg,gpointer data){
	switch (GST_MESSAGE_TYPE(msg)){
		case GST_MESSAGE_EOS:				//End Of Stream Signal
			g_print("End of Stream\n");
			g_main_loop_quit((GMainLoop*)data);     //Stop the main thread
			break;
		case GST_MESSAGE_ERROR:				//ANY error
			g_print("Any Error Handler Called");
			g_main_loop_quit((GMainLoop*)data);     //Stop the main thread
			break;
		default:
			break;
	}
	return TRUE;						//Signal Handled!
}
/*
on_pad_added_listener(element,pad,data)
called when the demuxer pad added , to connect dynamically with decoder*/
static void on_pad_added_listener(GstElement *element,GstPad*pad,gpointer data){
	GstPad *sink_pad;					//sink pad on decoder reference
	GstElement *decoder = (GstElement*)data;		//we pass the decoder below in g_signal_connect
	g_print("[DEBUG]on_pad_added_listener\n");
	sink_pad=gst_element_get_static_pad(decoder,"sink");	//get reference of the static sink pad
	gst_pad_link(pad,sink_pad);				//link to the pad with sink
	gst_object_unref(sink_pad);				//unreference the sink

}
/*
gboolean on_1000_timeout_listener
called every second , inform the bar(TODO: Add a progressBar) that 1sec is elapsed
@param pipeline, the main pipeline (top-level bin)*/
static gboolean on_1000_timeout_listener(GstElement *pipeline){
	gint64 pos,len;
	if(gst_element_query_position(pipeline,GST_FORMAT_TIME,&pos)
		&&gst_element_query_duration(pipeline,GST_FORMAT_TIME,&len)){
		g_print("Time :%"GST_TIME_FORMAT"/ %"GST_TIME_FORMAT".\r",
			GST_TIME_ARGS(pos),GST_TIME_ARGS(len));
	}

	recallMe;
}
/*initialize_factories
@brief  	initializes an arbitary number of factories!
@param int num  the number of pair (GstElementFactory**,gchar *) pairs to initialize*/
static gboolean initialize_factories(int num,...){
	GstElementFactory**cursor;
	gchar * name;
	va_list args;
	va_start(args,num);
	while(num>0){
		num-=1;
		cursor=va_arg(args,GstElementFactory**);
		name=va_arg(args,gchar *);
		*cursor=gst_element_factory_find(name);
		if(!(*cursor)){
			g_print("[INFO] %s factory not found!",name);
			g_free(name);
			return FALSE;
		}
		g_print("[INFO] %s factory loaded\n",name);
		g_free(name);
	}
	return TRUE;
}
/*
initialize_element
@brief		An elegant way to initialize an arbitary number of elements given the name and the factory
@int num 	the number of groups of type(GstElementFactory*,GstElement **,gchar*)  */
static gboolean initialize_elements(int num , ...){
	GstElementFactory*temp_fact;
	GstElement       **temp_elem;
	gchar		 *name;
	va_list		 args;
	va_start(args,num);
	while(num>0){
		num-=1;
		temp_fact=(GstElementFactory*)va_arg(args,GstElementFactory*);
		temp_elem=(GstElement**)va_arg(args,GstElement**);
		name=(gchar*)va_arg(args,gchar*);
		*temp_elem=gst_element_factory_create(temp_fact,name);
		//g_free(name);
		if(!(*temp_elem)){
			g_print("[ERR] %s not loaded!\n",name);
			g_free(name);
			return FALSE;
		}
		else {
			g_print("[INFO] %s element loaded\n",name);
		}
		g_free(name);
	}
	return TRUE;

}
int main(int argc,char *argv[]){

	//-------------Version Numbers -------------------------------//
	guint 			major,		//major version number
				minor,		//--//
				nano,
				pico;
	//-----------Element Factories-------------------------------//
	GstElementFactory	*source_factory,
				*demuxer_factory,
				*decoder_factory,
				*converter_factory,
				*sink_factory;
	//----------Element References----------------------------------
	GstElement 		*pipeline,	//main pipeline
				*source,	//filesrc source
				*demuxer,	//ogg demuxer to audio/video
				*decoder,	//Vorbis decoder
				*conv,		//sound converter
				*sink;		//sound card sink
	//----------Misc-------------------------------------------------
	guint                   any_event_listener_watch_id;
	GMainLoop               *main_loop;     //main threads loop object
	GstBus 			*main_bus;	//main bus to submit the any_event_listener

	//Initialization
	gst_init	(&argc,&argv);				//initialize gstreamer
	gst_version	(&major,&minor,&nano,&pico);		//get version numbes to print...
	g_print		("[INFO] Gstreamer v%d.%d.%d.%d Ready...\n",major,minor,nano,pico);
	main_loop=g_main_loop_new(NULL,FALSE);
	if(argc!=2){
		g_printerr("arg 2 must be *.ogg filename!");
		return -1;
	}
	//Elements Initialization
	if(!(pipeline=gst_pipeline_new("audio-player"))){
		g_print("[ERR]Pipeline could not be constructed,terninate\n");
		return -1;
	}

	if(!(initialize_factories(5,
				&source_factory   ,g_strdup("filesrc"),
				&demuxer_factory  ,g_strdup("oggdemux"),
				&decoder_factory  ,g_strdup("vorbisdec"),
				&converter_factory,g_strdup("audioconvert"),
				&sink_factory     ,g_strdup("autoaudiosink")))){
			g_print("[ERR]One or more factories not found ,terminate.\n");
			return -1;
	}
	if(!(initialize_elements(5,
				source_factory,	 &source   ,g_strdup("file-source"),
				demuxer_factory	 ,&demuxer ,g_strdup("ogg-demuxer"),
				decoder_factory	 ,&decoder ,g_strdup("vorbis-decoder"),
				converter_factory,&conv    ,g_strdup("converter"),
				sink_factory	 ,&sink    ,g_strdup("audio-output")))){
			g_print("[ERR]One or more elements fail to load , terminate\n");
			return -1;
	}
	//MAIN
	g_object_set(G_OBJECT(source),"location",argv[1],NULL);
	g_print("[INFO] file %s set to source stream\n",(const char *)argv[1]);
	main_bus=gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	any_event_listener_watch_id=gst_bus_add_watch(main_bus,any_event_listener,main_loop);
	g_print("[INFO] any_event_listener set as main bus handler with id %d\n",any_event_listener_watch_id);
	gst_object_unref(main_bus);
	g_print("[INFO] bus release");

	gst_bin_add_many(GST_BIN(pipeline),
			source, //src to demuxer pad
			demuxer,//demuxer audio to decoder
			decoder,//decoder out to input pad of converter
			conv,	//raw audio from converters src to sink (audio card)
			sink,
			NULL); //NULL Terminated list of elements
	g_print("[INFO]elements added to pipeline...\n");
	gst_element_link(source,demuxer);
	gst_element_link_many(	decoder,
				conv,
				sink,
				NULL);//NULL terminated varargs
	g_print("[INFO]elements connected \n");
	g_signal_connect(demuxer,"pad-added",G_CALLBACK(on_pad_added_listener),decoder);
	g_print("[INFO]on_pad_added_listener set to handle messages of type 'pad-added'\n");
	g_timeout_add(1000,(GSourceFunc)on_1000_timeout_listener,pipeline);
	g_print("[INFO]on_1000_timeout_listener set to call in 200ms interval");

	gst_element_set_state(pipeline,GST_STATE_PLAYING);
	g_print("[SUCCESS] pipeline on state PLAYING...\n");
	g_print("[SUCCESS]Rollin..");
	g_main_loop_run(main_loop);


	g_print("Exit! ");
	return 0;

}
