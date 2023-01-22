
#ifndef _SUBMIT_H_
#define _SUBMIT_H_

/* An array of the following structs is handed to the
 * submit_to_opensensemap_multi function. */
struct osm {
  char * sensorid;
  float value;
};

int submit_to_wpd(char * sensorid, char * valuetype, float value);

int submit_to_opensensemap_multi(char * boxid, int arraysize, struct osm * arrayofosm);
/* This is a convenience function, calling opensensemap_multi
 * with a size 1 array internally. */
int submit_to_opensensemap(char * boxid, char * sensorid, float value);

#endif /* _SUBMIT_H_ */

