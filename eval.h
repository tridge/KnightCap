/* the evaluation coefficients are now kept in this vector to allow
   for easy manipulation by automatic learning algorithms. */

#if STORE_LEAF_POS || MID_KNIGHT
#define KONST(a,b) (a)
#else
#define KONST(a,b) (b)
#endif

#define BOARD_CONTROL_FACTOR KONST(coefficients[0], 1)
#define PIECE_VALUE_FACTOR KONST(coefficients[1], 1)
#define BISHOP_PAIR KONST(coefficients[2], 20)
#define WEAK_PAWN_ATTACK_VALUE KONST(coefficients[3], 5)
#define UNSTOPPABLE_PAWN KONST(coefficients[4], 300)
#define QUEEN_ADVANCE KONST(coefficients[5], 5)
#define DOUBLED_PAWN KONST(coefficients[6], 15)
#define CASTLE_BONUS KONST(coefficients[7], 30)
#define ROOK_ON_OPEN_FILE KONST(coefficients[8], 10)
#define ROOK_ON_HALF_OPEN_FILE KONST(coefficients[9], 3)
#define KNIGHT_OUTPOST KONST(coefficients[10], 20)
#define BISHOP_OUTPOST KONST(coefficients[11], 15)
#define WEAK_PAWN  KONST(coefficients[12], 10)
#define WEAK_PIECE  KONST(coefficients[13], 10)
#define CONNECTED_ROOKS KONST(coefficients[14], 7)
#define ODD_BISHOPS_PAWN_POS KONST(coefficients[15], 2)
#define NO_PAWNS KONST(coefficients[16], 90)
#define OPPOSITE_BISHOPS KONST(coefficients[17], 40)
#define PINNED_HUNG_PIECE KONST(coefficients[18], 110)
#define TRAPPED_STEP KONST(coefficients[19], 16)
#define TRAPPED_VALUE KONST(coefficients[20], 20)
#define EARLY_QUEEN_MOVEMENT KONST(coefficients[21], 10)
#define BLOCKED_PASSED_PAWN KONST(coefficients[22], 20)
#define KING_PASSED_PAWN_SUPPORT KONST(coefficients[23], 20)
#define PASSED_PAWN_ROOK_ATTACK KONST(coefficients[24], 15)
#define PASSED_PAWN_ROOK_SUPPORT KONST(coefficients[25], 25)
#define TRADE_BONUS KONST(coefficients[26], 4)
#define PAWN_DEFENCE KONST(coefficients[27], 15)
#define OPENING_KING_ADVANCE KONST(coefficients[28], 30)
#define MID_KING_ADVANCE KONST(coefficients[29], 10)
#define BLOCKED_DPAWN KONST(coefficients[30], 6)
#define BLOCKED_EPAWN KONST(coefficients[31], 6)
#define BLOCKED_KNIGHT KONST(coefficients[32], 5)
#define USELESS_PIECE KONST(coefficients[33], 10)
#define DRAW_VALUE KONST(coefficients[34], -10)
#define NO_MATERIAL KONST(coefficients[35], 100)
#define MATING_POSITION KONST(coefficients[36], 100)
#define WEAK_ADVANCE_FACTOR KONST(coefficients[37], 0)
#define IPAWN_ADVANCE   38
#define IPAWN_POS       45
#define IENDING_KPOS 109
#define IROOK_POS    117
#define IBISHOP_XRAY 181
#define IKNIGHT_POS  186
#define IPOS_BASE    250
#define IPOS_KINGSIDE 314
#define IPOS_QUEENSIDE 378
#define IHUNG_VALUE     442
#define IBISHOP_MOBILITY 459
#define IROOK_MOBILITY 469
#define IQUEEN_MOBILITY 479
#define IKING_MOBILITY 489
#define IKNIGHT_SMOBILITY 499
#define IBISHOP_SMOBILITY 509
#define IROOK_SMOBILITY 519
#define IQUEEN_SMOBILITY 529
#define IKING_SMOBILITY 539
#define PAWN_ADVANCE_FACTOR KONST(coefficients[549], 0)
#define IPIECE_VALUES 550
#define HUNG_PIECE_FACTOR KONST(coefficients[557], 1)
#define KING_ATTACK_COMPUTER KONST(coefficients[558], 20)
#define ATTACK_VALUE KONST(coefficients[559], 2)
#define IPAWN_DEFENCE 560
#define ISOLATED_PAWN KONST(coefficients[572], 5)
#define KING_ATTACK_OPPONENT KONST(coefficients[573], 7)
#define THREAT KONST(coefficients[574], 33)
#define MEGA_WEAK_PAWN KONST(coefficients[575], 5)
#define OVERPROTECTION KONST(coefficients[576], 2)
#define __TOTAL_COEFFS__  577
