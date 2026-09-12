/**************************************************************************/
/*  irc_numerics.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

// IRC numeric reply codes from RFC 1459, RFC 2812, and IRCv3

namespace IRCNumerics {

// Connection Registration (001-099)
constexpr int RPL_WELCOME = 1;
constexpr int RPL_YOURHOST = 2;
constexpr int RPL_CREATED = 3;
constexpr int RPL_MYINFO = 4;
constexpr int RPL_ISUPPORT = 5; // Also known as RPL_BOUNCE in some implementations
constexpr int RPL_SNOMASK = 8;
constexpr int RPL_STATMEMTOT = 9;
constexpr int RPL_STATMEM = 10;

// Server Status (200-299)
constexpr int RPL_TRACELINK = 200;
constexpr int RPL_TRACECONNECTING = 201;
constexpr int RPL_TRACEHANDSHAKE = 202;
constexpr int RPL_TRACEUNKNOWN = 203;
constexpr int RPL_TRACEOPERATOR = 204;
constexpr int RPL_TRACEUSER = 205;
constexpr int RPL_TRACESERVER = 206;
constexpr int RPL_TRACENEWTYPE = 208;
constexpr int RPL_STATSLINKINFO = 211;
constexpr int RPL_STATSCOMMANDS = 212;
constexpr int RPL_STATSCLINE = 213;
constexpr int RPL_STATSNLINE = 214;
constexpr int RPL_STATSILINE = 215;
constexpr int RPL_STATSKLINE = 216;
constexpr int RPL_STATSYLINE = 218;
constexpr int RPL_ENDOFSTATS = 219;
constexpr int RPL_UMODEIS = 221;
constexpr int RPL_STATSDLINE = 225;
constexpr int RPL_SERVLIST = 234;
constexpr int RPL_SERVLISTEND = 235;
constexpr int RPL_STATSLLINE = 241;
constexpr int RPL_STATSUPTIME = 242;
constexpr int RPL_STATSOLINE = 243;
constexpr int RPL_STATSHLINE = 244;
constexpr int RPL_LUSERCLIENT = 251;
constexpr int RPL_LUSEROP = 252;
constexpr int RPL_LUSERUNKNOWN = 253;
constexpr int RPL_LUSERCHANNELS = 254;
constexpr int RPL_LUSERME = 255;
constexpr int RPL_ADMINME = 256;
constexpr int RPL_ADMINLOC1 = 257;
constexpr int RPL_ADMINLOC2 = 258;
constexpr int RPL_ADMINEMAIL = 259;
constexpr int RPL_TRACELOG = 261;
constexpr int RPL_ENDOFTRACE = 262;
constexpr int RPL_LOAD2HI = 263;
constexpr int RPL_LOCALUSERS = 265;
constexpr int RPL_GLOBALUSERS = 266;
constexpr int RPL_WHOISCERTFP = 276;
constexpr int RPL_NONE = 300;

// User Status (300-399)
constexpr int RPL_AWAY = 301;
constexpr int RPL_USERHOST = 302;
constexpr int RPL_ISON = 303;
constexpr int RPL_UNAWAY = 305;
constexpr int RPL_NOWAWAY = 306;
constexpr int RPL_WHOISUSER = 311;
constexpr int RPL_WHOISSERVER = 312;
constexpr int RPL_WHOISOPERATOR = 313;
constexpr int RPL_WHOWASUSER = 314;
constexpr int RPL_ENDOFWHO = 315;
constexpr int RPL_WHOISCHANOP = 316;
constexpr int RPL_WHOISIDLE = 317;
constexpr int RPL_ENDOFWHOIS = 318;
constexpr int RPL_WHOISCHANNELS = 319;
constexpr int RPL_WHOISSPECIAL = 320;
constexpr int RPL_LISTSTART = 321;
constexpr int RPL_LIST = 322;
constexpr int RPL_LISTEND = 323;
constexpr int RPL_CHANNELMODEIS = 324;
constexpr int RPL_CREATIONTIME = 329;
constexpr int RPL_WHOISACCOUNT = 330;
constexpr int RPL_NOTOPIC = 331;
constexpr int RPL_TOPIC = 332;
constexpr int RPL_TOPICWHOTIME = 333;
constexpr int RPL_INVITELIST = 336;
constexpr int RPL_ENDOFINVITELIST = 337;
constexpr int RPL_WHOISACTUALLY = 338;
constexpr int RPL_INVITING = 341;
constexpr int RPL_SUMMONING = 342;
constexpr int RPL_VERSION = 351;
constexpr int RPL_WHOREPLY = 352;
constexpr int RPL_NAMREPLY = 353;
constexpr int RPL_LINKS = 364;
constexpr int RPL_ENDOFLINKS = 365;
constexpr int RPL_ENDOFNAMES = 366;
constexpr int RPL_BANLIST = 367;
constexpr int RPL_ENDOFBANLIST = 368;
constexpr int RPL_ENDOFWHOWAS = 369;
constexpr int RPL_INFO = 371;
constexpr int RPL_MOTD = 372;
constexpr int RPL_ENDOFINFO = 374;
constexpr int RPL_MOTDSTART = 375;
constexpr int RPL_ENDOFMOTD = 376;
constexpr int RPL_WHOISHOST = 378;
constexpr int RPL_WHOISMODES = 379;
constexpr int RPL_YOUREOPER = 381;
constexpr int RPL_REHASHING = 382;
constexpr int RPL_TIME = 391;
constexpr int RPL_USERSSTART = 392;
constexpr int RPL_USERS = 393;
constexpr int RPL_ENDOFUSERS = 394;
constexpr int RPL_NOUSERS = 395;

// Errors (400-599)
constexpr int ERR_NOSUCHNICK = 401;
constexpr int ERR_NOSUCHSERVER = 402;
constexpr int ERR_NOSUCHCHANNEL = 403;
constexpr int ERR_CANNOTSENDTOCHAN = 404;
constexpr int ERR_TOOMANYCHANNELS = 405;
constexpr int ERR_WASNOSUCHNICK = 406;
constexpr int ERR_TOOMANYTARGETS = 407;
constexpr int ERR_NOORIGIN = 409;
constexpr int ERR_NORECIPIENT = 411;
constexpr int ERR_NOTEXTTOSEND = 412;
constexpr int ERR_NOTOPLEVEL = 413;
constexpr int ERR_WILDTOPLEVEL = 414;
constexpr int ERR_UNKNOWNCOMMAND = 421;
constexpr int ERR_NOMOTD = 422;
constexpr int ERR_NOADMININFO = 423;
constexpr int ERR_FILEERROR = 424;
constexpr int ERR_NONICKNAMEGIVEN = 431;
constexpr int ERR_ERRONEUSNICKNAME = 432;
constexpr int ERR_NICKNAMEINUSE = 433;
constexpr int ERR_NICKCOLLISION = 436;
constexpr int ERR_UNAVAILRESOURCE = 437;
constexpr int ERR_USERNOTINCHANNEL = 441;
constexpr int ERR_NOTONCHANNEL = 442;
constexpr int ERR_USERONCHANNEL = 443;
constexpr int ERR_NOLOGIN = 444;
constexpr int ERR_SUMMONDISABLED = 445;
constexpr int ERR_USERSDISABLED = 446;
constexpr int ERR_NOTREGISTERED = 451;
constexpr int ERR_NEEDMOREPARAMS = 461;
constexpr int ERR_ALREADYREGISTRED = 462;
constexpr int ERR_NOPERMFORHOST = 463;
constexpr int ERR_PASSWDMISMATCH = 464;
constexpr int ERR_YOUREBANNEDCREEP = 465;
constexpr int ERR_KEYSET = 467;
constexpr int ERR_CHANNELISFULL = 471;
constexpr int ERR_UNKNOWNMODE = 472;
constexpr int ERR_INVITEONLYCHAN = 473;
constexpr int ERR_BANNEDFROMCHAN = 474;
constexpr int ERR_BADCHANNELKEY = 475;
constexpr int ERR_BADCHANMASK = 476;
constexpr int ERR_NOCHANMODES = 477;
constexpr int ERR_BANLISTFULL = 478;
constexpr int ERR_NOPRIVILEGES = 481;
constexpr int ERR_CHANOPRIVSNEEDED = 482;
constexpr int ERR_CANTKILLSERVER = 483;
constexpr int ERR_RESTRICTED = 484;
constexpr int ERR_UNIQOPPRIVSNEEDED = 485;
constexpr int ERR_NOOPERHOST = 491;
constexpr int ERR_UMODEUNKNOWNFLAG = 501;
constexpr int ERR_USERSDONTMATCH = 502;

// SASL (IRCv3)
constexpr int RPL_LOGGEDIN = 900;
constexpr int RPL_LOGGEDOUT = 901;
constexpr int ERR_NICKLOCKED = 902;
constexpr int RPL_SASLSUCCESS = 903;
constexpr int ERR_SASLFAIL = 904;
constexpr int ERR_SASLTOOLONG = 905;
constexpr int ERR_SASLABORTED = 906;
constexpr int ERR_SASLALREADY = 907;
constexpr int RPL_SASLMECHS = 908;

// MONITOR (IRCv3.3)
constexpr int RPL_MONONLINE = 730;
constexpr int RPL_MONOFFLINE = 731;
constexpr int RPL_MONLIST = 732;
constexpr int RPL_ENDOFMONLIST = 733;
constexpr int ERR_MONLISTFULL = 734;

// IRCv3 Capabilities - Common capability strings
namespace Capabilities {
constexpr const char *ACCOUNT_NOTIFY = "account-notify";
constexpr const char *AWAY_NOTIFY = "away-notify";
constexpr const char *BATCH = "batch";
constexpr const char *CAP_NOTIFY = "cap-notify";
constexpr const char *CHGHOST = "chghost";
constexpr const char *EXTENDED_JOIN = "extended-join";
constexpr const char *INVITE_NOTIFY = "invite-notify";
constexpr const char *LABELED_RESPONSE = "labeled-response";
constexpr const char *MESSAGE_TAGS = "message-tags";
constexpr const char *MULTI_PREFIX = "multi-prefix";
constexpr const char *SASL = "sasl";
constexpr const char *SERVER_TIME = "server-time";
constexpr const char *SETNAME = "setname";
constexpr const char *USERHOST_IN_NAMES = "userhost-in-names";
constexpr const char *ACCOUNT_TAG = "account-tag";
constexpr const char *ECHO_MESSAGE = "echo-message";
constexpr const char *MONITOR = "monitor";
constexpr const char *STANDARD_REPLIES = "standard-replies";
constexpr const char *CHATHISTORY = "draft/chathistory";
constexpr const char *MULTILINE = "draft/multiline";
constexpr const char *READ_MARKER = "draft/read-marker";
constexpr const char *TYPING = "typing";
constexpr const char *REACT = "draft/react";
constexpr const char *ACCOUNT_REGISTRATION = "draft/account-registration";
} // namespace Capabilities

// IRCv3.4 Standard Reply Commands
namespace StandardReplies {
constexpr const char *FAIL = "FAIL";
constexpr const char *WARN = "WARN";
constexpr const char *NOTE = "NOTE";
} // namespace StandardReplies

} // namespace IRCNumerics
