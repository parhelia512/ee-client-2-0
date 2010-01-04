typedef union {
   char              c;
   int               i;
   const char *      s;
   char *            str;
   double            f;
   StmtNode *        stmt;
   ExprNode *        expr;
   SlotAssignNode *  slist;
   VarNode *         var;
   SlotDecl          slot;
   InternalSlotDecl  intslot;
   ObjectBlockDecl   odcl;
   ObjectDeclNode *  od;
   AssignDecl        asn;
   IfStmtNode *      ifnode;
} YYSTYPE;
#define	rwDEFINE	258
#define	rwENDDEF	259
#define	rwDECLARE	260
#define	rwDECLARESINGLETON	261
#define	rwBREAK	262
#define	rwELSE	263
#define	rwCONTINUE	264
#define	rwGLOBAL	265
#define	rwIF	266
#define	rwNIL	267
#define	rwRETURN	268
#define	rwWHILE	269
#define	rwDO	270
#define	rwENDIF	271
#define	rwENDWHILE	272
#define	rwENDFOR	273
#define	rwDEFAULT	274
#define	rwFOR	275
#define	rwDATABLOCK	276
#define	rwSWITCH	277
#define	rwCASE	278
#define	rwSWITCHSTR	279
#define	rwCASEOR	280
#define	rwPACKAGE	281
#define	rwNAMESPACE	282
#define	rwCLASS	283
#define	rwASSERT	284
#define	ILLEGAL_TOKEN	285
#define	CHRCONST	286
#define	INTCONST	287
#define	TTAG	288
#define	VAR	289
#define	IDENT	290
#define	TYPE	291
#define	DOCBLOCK	292
#define	STRATOM	293
#define	TAGATOM	294
#define	FLTCONST	295
#define	opINTNAME	296
#define	opINTNAMER	297
#define	opMINUSMINUS	298
#define	opPLUSPLUS	299
#define	STMT_SEP	300
#define	opSHL	301
#define	opSHR	302
#define	opPLASN	303
#define	opMIASN	304
#define	opMLASN	305
#define	opDVASN	306
#define	opMODASN	307
#define	opANDASN	308
#define	opXORASN	309
#define	opORASN	310
#define	opSLASN	311
#define	opSRASN	312
#define	opCAT	313
#define	opEQ	314
#define	opNE	315
#define	opGE	316
#define	opLE	317
#define	opAND	318
#define	opOR	319
#define	opSTREQ	320
#define	opCOLONCOLON	321
#define	opMDASN	322
#define	opNDASN	323
#define	opNTASN	324
#define	opSTRNE	325
#define	UNARY	326


extern YYSTYPE CMDlval;
