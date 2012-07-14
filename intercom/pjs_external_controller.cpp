#include "pjs_external_controller.h"


pjs_external_controller::pjs_external_controller(pjs_global &global,pjs_external_controller_callback &callback):
	m_global(global),m_callback(callback)
{

}

pjs_external_controller::~pjs_external_controller()
{

}


