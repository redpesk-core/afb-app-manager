# Debugging

When AGL_DEVEL is ON, the framework include several settings
in the unit generated files that can be used for debugging.

allows  several


/run/platform/debug/homescreen-service@0.1.env
/etc/afm/widget.env.d/

	on_environment_enum(config, SET_TRACEREQ, "AFB_TRACEREQ", afb_hook_flags_xreq_from_text);
	on_environment_enum(config, SET_TRACEEVT, "AFB_TRACEEVT", afb_hook_flags_evt_from_text);
	on_environment_enum(config, SET_TRACESES, "AFB_TRACESES", afb_hook_flags_session_from_text);
	on_environment_enum(config, SET_TRACEAPI, "AFB_TRACEAPI", afb_hook_flags_api_from_text);
	on_environment_enum(config, SET_TRACEGLOB, "AFB_TRACEGLOB", afb_hook_flags_global_from_text);
