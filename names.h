static struct coefficient_name {
	char *name;
	int index;
} coefficient_names[] = {
{"BOARD_CONTROL_FACTOR", 0},
{"PIECE_VALUE_FACTOR", 1},
{"BISHOP_PAIR", 2},
{"WEAK_PAWN_ATTACK_VALUE", 3},
{"UNSTOPPABLE_PAWN", 4},
{"QUEEN_ADVANCE", 5},
{"DOUBLED_PAWN", 6},
{"CASTLE_BONUS", 7},
{"ROOK_ON_OPEN_FILE", 8},
{"ROOK_ON_HALF_OPEN_FILE", 9},
{"KNIGHT_OUTPOST", 10},
{"BISHOP_OUTPOST", 11},
{"WEAK_PAWN ", 12},
{"WEAK_PIECE ", 13},
{"CONNECTED_ROOKS", 14},
{"ODD_BISHOPS_PAWN_POS", 15},
{"NO_PAWNS", 16},
{"OPPOSITE_BISHOPS", 17},
{"PINNED_HUNG_PIECE", 18},
{"TRAPPED_STEP", 19},
{"TRAPPED_VALUE", 20},
{"EARLY_QUEEN_MOVEMENT", 21},
{"BLOCKED_PASSED_PAWN", 22},
{"KING_PASSED_PAWN_SUPPORT", 23},
{"PASSED_PAWN_ROOK_ATTACK", 24},
{"PASSED_PAWN_ROOK_SUPPORT", 25},
{"TRADE_BONUS", 26},
{"PAWN_DEFENCE", 27},
{"OPENING_KING_ADVANCE", 28},
{"MID_KING_ADVANCE", 29},
{"BLOCKED_DPAWN", 30},
{"BLOCKED_EPAWN", 31},
{"BLOCKED_KNIGHT", 32},
{"USELESS_PIECE", 33},
{"DRAW_VALUE", 34},
{"NO_MATERIAL", 35},
{"MATING_POSITION", 36},
{"WEAK_ADVANCE_FACTOR", 37},
{"IPAWN_ADVANCE",   38},
{"IPAWN_POS",       45},
{"IENDING_KPOS", 109},
{"IROOK_POS",    117},
{"IBISHOP_XRAY", 181},
{"IKNIGHT_POS",  186},
{"IPOS_BASE",    250},
{"IPOS_KINGSIDE", 314},
{"IPOS_QUEENSIDE", 378},
{"IHUNG_VALUE",     442},
{"IBISHOP_MOBILITY", 459},
{"IROOK_MOBILITY", 469},
{"IQUEEN_MOBILITY", 479},
{"IKING_MOBILITY", 489},
{"IKNIGHT_SMOBILITY", 499},
{"IBISHOP_SMOBILITY", 509},
{"IROOK_SMOBILITY", 519},
{"IQUEEN_SMOBILITY", 529},
{"IKING_SMOBILITY", 539},
{"PAWN_ADVANCE_FACTOR", 549},
{"IPIECE_VALUES", 550},
{"HUNG_PIECE_FACTOR", 557},
{"KING_ATTACK_COMPUTER", 558},
{"ATTACK_VALUE", 559},
{"IPAWN_DEFENCE", 560},
{"ISOLATED PAWN", 572},
{"KING_ATTACK_OPPONENT", 573},
{"THREAT", 574},
{"MEGA_WEAK_PAWN", 575},
{"OVERPROTECTION", 576},
{NULL, __TOTAL_COEFFS__}};