/*
 * DigestAuthentication.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef DIGESTAUTHENTICATION_H_
#define DIGESTAUTHENTICATION_H_


//
#include <sys/time.h>
#include "strDup.h"

// A class used for digest authentication.
// The "realm", and "nonce" fields are supplied by the server
// (in a "401 Unauthorized" response).
// The "username" and "password" fields are supplied by the client.
class Authenticator {
public:
  Authenticator();
  Authenticator(char const* username, char const* password, bool passwordIsMD5 = false);
      // If "passwordIsMD5" is True, then "password" is actually the value computed
      // by md5(<username>:<realm>:<actual-password>)
  Authenticator(const Authenticator& orig);
  Authenticator& operator=(const Authenticator& rightSide);
  virtual ~Authenticator();

  void reset();
  void setRealmAndNonce(char const* realm, char const* nonce);
  void setRealmAndRandomNonce(char const* realm);
      // as above, except that the nonce is created randomly.
      // (This is used by servers.)
  void setUsernameAndPassword(char const* username, char const* password, bool passwordIsMD5 = false);
      // If "passwordIsMD5" is True, then "password" is actually the value computed
      // by md5(<username>:<realm>:<actual-password>)

  char const* realm() const { return fRealm; }
  char const* nonce() const { return fNonce; }
  char const* username() const { return fUsername; }
  char const* password() const { return fPassword; }

  char const* computeDigestResponse(char const* cmd, char const* url) const;
  void reclaimDigestResponse(char const* responseStr) const;

private:
  void resetRealmAndNonce();
  void resetUsernameAndPassword();
  void assignRealmAndNonce(char const* realm, char const* nonce);
  void assignUsernameAndPassword(char const* username, char const* password, bool passwordIsMD5);
  void assign(char const* realm, char const* nonce,
	      char const* username, char const* password, bool passwordIsMD5);

private:
  char* fRealm; char* fNonce;
  char* fUsername; char* fPassword;
  bool fPasswordIsMD5;
};


#endif /* DIGESTAUTHENTICATION_H_ */
