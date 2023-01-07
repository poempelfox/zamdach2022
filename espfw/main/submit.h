
#ifndef _SUBMIT_H_
#define _SUBMIT_H_

int submit_to_wpd(char * sensorid, char * valuetype, float value);
int submit_to_opensensemap(char * boxid, char * sensorid, float value);

#endif /* _SUBMIT_H_ */

