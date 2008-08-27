/* Copyright (C) 2003-2007 Datapark corp. All rights reserved.
   Copyright (C) 2000-2002 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/
#include "dpsearch.h"
#include "dps_xmalloc.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

int main(int argc, char **argv, char **envp) {
	char		template_name[PATH_MAX+4] = "";
	char            *template_filename = NULL;
	char		*query_string = NULL;
	char		self[1024] = "";
	const char	*env, *lcharset, *bcharset, *htok, *conf_dir;
	char            *last;
	DPS_ENV		*Env;
	DPS_AGENT	*Agent;
	DPS_CHARSET	*lcs = NULL, *bcs = NULL, *sys_int;
	DPS_CONV	lc_uni, uni_bc;
	DPS_CONV	lc_uni_text, uni_bc_text;
	DPS_DOCUMENT	*Doc;
	DPS_RESULT	*Res;
	DPS_HTMLTOK	tag;
	DPS_VARLIST	query_vars;
	int		httpd = 0, res;
	char		*HDoc = NULL;
	char		*HEnd = NULL;
	char            ch;
	const char            *content_type;
#ifdef WITH_PARSER
	DPS_PARSER	*Parser;
#endif
	
	/* Output Content-type if under HTTPD	 */
	/* Some servers do not pass QUERY_STRING */
	/* if the query was empty, so check	 */
	/* REQUEST_METHOD too     to be safe     */
	
	if(getenv("QUERY_STRING")||getenv("REQUEST_METHOD")){
		httpd=1;
	}
	if (!(conf_dir=getenv("DPS_ETC_DIR")))
		conf_dir=DPS_CONF_DIR;
	
	DpsInit(argc, argv, envp);
	Env = DpsEnvInit(NULL);
	if (Env == NULL) {
	  if(httpd){
	    printf("Content-Type: text/plain\r\n\r\n");
	  }
	  printf("Can't alloc Env\n");
	  exit(0);
	}
	DpsVarListInit(&query_vars);
	Agent = DpsAgentInit(NULL, Env, 0);
	if (Agent == NULL) {
	  if(httpd){
	    printf("Content-Type: text/plain\r\n\r\n");
	  }
	  printf("Can't alloc Agent\n");
	  exit(0);
	}
	DpsVarListAddEnviron(&Env->Vars,"ENV");

	/* Detect self and template name */
	if((env = getenv("DPSEARCH_DOCTEMPLATE")))
		dps_strncpy(template_name, env, sizeof(template_name) - 1);
	else if((env = getenv("PATH_INFO")) && env[0])
		dps_strncpy(template_name, env + 1, sizeof(template_name) - 1);

	if((env=getenv("QUERY_STRING"))){
	        
	        query_string = (char*)DpsRealloc(query_string, dps_strlen(env) + 2);
		if (query_string == NULL) {
		  if(httpd){
		    printf("Content-Type: text/plain\r\n\r\n");
		  }
		  printf("Can't alloc query_string\n");
		  exit(0);
		}
		dps_strncpy(query_string, env, dps_strlen(env) + 1);

		/* Unescape and save variables from QUERY_STRING */
		/* Env->Vars will have unescaped values however  */
		DpsParseQueryString(Agent,&Env->Vars,query_string);
	
		template_filename = (char*)DpsStrdup(DpsVarListFindStr(&Env->Vars, "tmplt", ""));

		if((env=getenv("REDIRECT_STATUS"))){

			/* Check Apache internal redirect  */
			/* via   "AddHandler" and "Action" */

			dps_strncpy(self,(env=getenv("REDIRECT_URL"))?env:"storedoc.cgi",sizeof(self)-1);
			if(!template_name[0]){
				dps_strncpy(template_name,(env=getenv("PATH_TRANSLATED"))?env:"",sizeof(template_name)-1);
			}
		}else{
			/* CGI executed without Apache internal redirect */

			/* Detect $Self variable with OS independant SLASHES */
			dps_strncpy(self,(env=getenv("SCRIPT_NAME"))?env:"storedoc.cgi",sizeof(self)-1);

			if(!template_name[0]){
				char *s,*e;
				
				/*This is with OS specific SLASHES */
				env=((env=getenv("SCRIPT_FILENAME"))?env:"storedoc.cgi");

				if(strcmp(conf_dir, ".")) {
					/* Take from the config directory */
					dps_snprintf(template_name,sizeof(template_name)-1,"%s/%s", 
						     conf_dir,(s=strrchr(env,DPSSLASH))?(s+1):(self));
				}else{
					/* Take from the current directory */
					dps_strncpy(template_name,env,sizeof(template_name)-1);
				}

				/* Find right slash if it presents */
				s=((s=strrchr(template_name,DPSSLASH))?s:template_name);

				/* Find .cgi substring */
				if((e = strstr(s,".cgi")) != NULL) {
				  /* Replace ".cgi" with ".htm" */
				  e[1]='h';e[2]='t';e[3]='m';
				}else{
				  dps_strcat(s, ".htm");
/*				  dps_strcpy(template_name, "storedoc.htm");*/
				}
				e = strrchr(template_name, DPSSLASH);
				if (e == NULL) e = template_name;
				if (*template_filename == '\0') { 
				  DPS_FREE(template_filename);
				  template_filename = (char*)DpsStrdup(e + 1);
				} else {
				  dps_snprintf(e + 1, sizeof(template_name) - (e - template_name), "%s", template_filename);
				}
			}
		}
	}else{
		/* Executed from command line     */
		/* or under server which does not */
		/* pass an empty QUERY_STRING var */
	  if(argv[1]) {
	    query_string = (char*)DpsRealloc(query_string, dps_strlen(argv[1]) + 10);
	    if (query_string == NULL) {
	      if(httpd){
		printf("Content-Type: text/plain\r\n\r\n");
	      }
	      printf("Can't realloc query_string\n");
	      exit(0);
	    }
	    dps_snprintf(query_string,sizeof(query_string)-1,"q=%s",argv[1]);
	  } else {
	    query_string = (char*)DpsRealloc(query_string, 1024);
	    if (query_string == NULL) {
	      if(httpd){
		printf("Content-Type: text/plain\r\n\r\n");
	      }
	      printf("Can't realloc query_string\n");
	      exit(0);
	    }
	    sprintf(query_string, "q=");
	  }

	  /* Unescape and save variables from QUERY_STRING */
	  /* Env->Vars will have unescaped values however  */
	  DpsParseQueryString(Agent,&Env->Vars,query_string);

	  DPS_FREE(template_filename);
	  template_filename = (char*)DpsStrdup(DpsVarListFindStr(&Env->Vars, "tmplt", ""));
	  if (*template_filename == '\0') { DPS_FREE(template_filename); template_filename = (char*)DpsStrdup("search.htm"); }

	  if(!template_name[0]) dps_snprintf(template_name,sizeof(template_name), "%s/%s", conf_dir, "storedoc.htm");
	}

	DpsVarListReplaceStr(&Agent->Conf->Vars, "tmplt", template_filename);
	DPS_FREE(template_filename);

	Agent->st_tmpl.Env_Vars = &Env->Vars;

	DpsURLNormalizePath(template_name);
	
	if(strncmp(template_name, conf_dir, dps_strlen(conf_dir)) 
	   || (res = DpsTemplateLoad(Agent, Env, &Agent->st_tmpl, template_name))) {
	  if (strcmp(template_name, "storedoc.htm")) { /* trying load default template */
	    dps_snprintf(template_name, sizeof(template_name), "%s/%s", conf_dir, "storedoc.htm");

	    if ((res = DpsTemplateLoad(Agent, Env, &Agent->st_tmpl, template_name))) {
		if(httpd)printf("Content-Type: text/plain\r\n\r\n");
		printf("%s\n",Env->errstr);
		DpsEnvFree(Env);
		DPS_FREE(query_string);
		return(0);
	    }
	  } else {
		if(httpd)printf("Content-Type: text/plain\r\n\r\n");
		printf("%s\n",Env->errstr);
		DpsEnvFree(Env);
		DPS_FREE(query_string);
		return(0);
	  }
	}
	
	DpsVarListAddStr(&Env->Vars,"QUERY_STRING",query_string);
	DpsVarListAddStr(&Env->Vars,"self",self);
	
	DpsParseQueryString(Agent,&Env->Vars,query_string);
	Agent->Flags = Env->Flags;
	Agent->flags |= DPS_FLAG_UNOCON;
	Env->flags |= DPS_FLAG_UNOCON;
	DpsVarListAddLst(&Agent->Vars, &Env->Vars, NULL, "*");
	DpsSetLogLevel(NULL, DpsVarListFindInt(&Agent->Vars, "LogLevel", 0));
	DpsOpenLog("storedoc.cgi", Env, !httpd);

	lcharset = DpsVarListFindStr(&Env->Vars, "CS", "");
	if (lcharset == NULL || (!strcmp(lcharset, ""))) {
		lcharset=DpsVarListFindStr(&Env->Vars,"LocalCharset","iso-8859-1");
	}
	lcs=DpsGetCharSet(lcharset);
	bcharset=DpsVarListFindStr(&Env->Vars,"BrowserCharset","iso-8859-1");
	bcs=DpsGetCharSet(bcharset);

	if(httpd){
		if(!bcs){
			printf("Content-Type: text/plain\r\n\r\n");
			printf("Unknown BrowserCharset '%s' in template '%s'\n",bcharset,template_name);
			exit(0);
		} else if (!lcs) {
			printf("Content-Type: text/plain\r\n\r\n");
			printf("Unknown LocalCharset '%s' in template '%s'\n",lcharset,template_name);
			exit(0);
		}else{
			printf("Content-type: text/html; charset=%s\r\n\r\n",bcharset);
		}
	}else{
		if(!bcs){
			printf("Unknown BrowserCharset '%s' in template '%s'\n",bcharset,template_name);
			exit(0);
		}
		if(!lcs){
			printf("Unknown LocalCharset '%s' in template '%s'\n",lcharset,template_name);
			exit(0);
		}
	}
	
	/* Now start displaying template*/
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->st_tmpl, "top");
	
	/* UnStore Doc, Highlight and Display */
	
	Doc=DpsDocInit(NULL);
	Res=DpsResultInit(NULL);
	if (Res == NULL) {
	  if(httpd){
	    printf("Content-Type: text/plain\r\n\r\n");
	  }
	  printf("Can't alloc Result\n");
	  exit(0);
	}
	
	DpsPrepare(Agent, Res);
	DpsWWLBoolItems(Res);
	DpsVarListReplaceStr(&Doc->Sections, "URL_ID", DpsVarListFindStr(&Env->Vars, "rec_id", "0"));
	
	content_type = DpsVarListFindStr(&Env->Vars, "CT", "text/html");

#ifdef WITH_PARSER
	Parser = DpsParserFind(&Env->Parsers, content_type);
	if (Parser) {
		content_type = Parser->to_mime ? Parser->to_mime : "text/html";
	}
#endif

	DpsAgentStoredConnect(Agent);
	
	DpsUnStoreDoc(Agent, Doc, NULL);
	Res->num_rows = 1;
	Res->Doc = Doc;

	if(!Doc->Buf.content) {
	  char *origurl = DpsVarListFindStr(&Env->Vars, "DU", NULL);
	  if (origurl != NULL) {
	    DpsVarListReplaceStr(&Doc->Sections, "URL", origurl);
	    Doc->Buf.max_size = (size_t)DpsVarListFindInt(&Agent->Vars, "MaxDocSize", DPS_MAXDOCSIZE);
	    DpsURLParse(&Doc->CurURL, origurl);
	    DpsDocAddDocExtraHeaders(Agent, Doc);
	    DpsDocLookupConn(Agent, Doc);
	    if (DpsGetURL(Agent, Doc, origurl) != DPS_OK) goto fin;
	  }
	  else goto fin;
	}
	DpsConvert(Env, &Agent->Vars, Res, lcs, bcs);
	DpsVarListAddLst(&Env->Vars, &Doc->Sections, NULL, "*");

	if (*DpsVarListFindStr(&Env->Vars, "HlBeg", "") == '\0') {
	  DpsVarListAddStr(&Env->Vars, "document", Doc->Buf.content);
	} else {
	  	
	  HEnd = HDoc = (char*)DpsMalloc(DPS_MAXDOCSIZE + 32);
	  if (HDoc == NULL) goto fin;
	  *HEnd = '\0';
	
	  if (strncasecmp(content_type, "text/plain", 10) == 0) {
	    sprintf(HEnd, "<pre>\n");
	    HEnd += dps_strlen(HEnd);
	  }

	  sys_int = DpsGetCharSet("sys-int");
	  DpsConvInit(&lc_uni, lcs, sys_int, Env->CharsToEscape, DPS_RECODE_HTML);
	  DpsConvInit(&uni_bc, sys_int, bcs, Env->CharsToEscape, DPS_RECODE_HTML);
	  DpsConvInit(&lc_uni_text, lcs, sys_int, Env->CharsToEscape, DPS_RECODE_TEXT);
	  DpsConvInit(&uni_bc_text, sys_int, bcs, Env->CharsToEscape, DPS_RECODE_TEXT);
	  
	  DpsHTMLTOKInit(&tag);
  
	  for(htok = DpsHTMLToken(Doc->Buf.content, (const char **)&last, &tag) ; htok ;) {
		switch(tag.type) {
		case DPS_HTML_COM:
		case DPS_HTML_TAG:
			dps_memmove(HEnd, htok, (size_t)(last - htok));
			HEnd+=last-htok;
			HEnd[0]='\0';

			DpsHTMLParseTag(Agent, &tag, Doc);
			break;
		case DPS_HTML_TXT:
		        ch = *last; *last = '\0';
			if (tag.title || tag.script) {
			  sprintf(HEnd, "%s", DpsHlConvert(NULL, htok, &lc_uni_text, &uni_bc_text));
			} else {
			  sprintf(HEnd, "%s", DpsHlConvert(&Res->WWList, htok, &lc_uni, &uni_bc));
			}
			HEnd=DPS_STREND(HEnd);
			*last = ch;
			break;
		}
		htok = DpsHTMLToken(NULL, (const char **)&last, &tag);
	  }
	
	  if (strncasecmp(content_type, "text/plain", 10) == 0) {
	    sprintf(HEnd, "</pre>\n");
	    HEnd += dps_strlen(HEnd);
	  }
	  DpsVarListAddStr(&Env->Vars, "document", HDoc);
	}
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->st_tmpl, "result");
	
fin:
	Res->Doc = NULL;
	Res->num_rows = 0;
	DpsResultFree(Res);
	DpsDocFree(Doc);
	
	DpsTemplatePrint(Agent, (DPS_OUTPUTFUNCTION)&fprintf, stdout, NULL, 0, &Agent->st_tmpl, "bottom");
	DpsAgentFree(Agent);
	DpsEnvFree(Env);
	DPS_FREE(query_string);
	DPS_FREE(HDoc);

	return(0);
}
