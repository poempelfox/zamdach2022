
#ifndef _SUBMIT_H_
#define _SUBMIT_H_

/* An array of the following structs is handed to the
 * submit_to_opensensemap_multi or submit_to_wpd_multi
 * functions. Note that the value of sensorid for both
 * APIs is very different though. */
struct osm {
  const char * sensorid;
  float value;
};

/* Submits multiple values to the wetter.poempelfox.de API
 * in one HTTPS request */
int submit_to_wpd_multi(int arraysize, struct osm * arrayofosm);

/* This is a convenience function, calling ..._wpd_multi
 * with a size 1 array internally. */
int submit_to_wpd(char * sensorid, float value);

/* Submits multiple values to the opensensemap-API
 * (api.opensensemap.org) in one HTTPS request */
int submit_to_opensensemap_multi(char * boxid, int arraysize, struct osm * arrayofosm);

/* This is a convenience function, calling ..._opensensemap_multi
 * with a size 1 array internally. */
int submit_to_opensensemap(char * boxid, char * sensorid, float value);

#endif /* _SUBMIT_H_ */

