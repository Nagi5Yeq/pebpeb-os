#include <pts.h>
#include <sched.h>

const char toad[] =
    "..:.         ......::..  ..^7JYYJJJJJYJJJJ??7~^:..         ^~^^:........  "
    "      "
    ".....        .....:.. .^!?JPGB#BBBBBBGGGGGGGP5J7~:..        ^~^:......... "
    "      "
    "....           ..:...!P#########BBBBGGGGGGGGGP5Y?~:..       "
    ".^::...........     "
    "...             ...^Y#&&&&&##BBBBBBGPP555PPPPYY?!^:..        ...    "
    "............"
    "                ..:?B#&&&&&&##BBBBBGPP55PP55YJ?7~:...                "
    "..........."
    "                ..^5B#&&&&&###B##B#BBGGGGP5Y??"
    "!!^..                   "
    ".........."
    "                ..7PB#&&&&&#B###B#&#GGPPP55YJ7!!^:.                    "
    "........."
    "                 :75B#&&#BBGGPG#PBBY?77?JJJ7!7!~^^.                    "
    "........."
    "   .             .~5GG#BGYYJ?7PBY?7?YPPP5YJ7~:~!::..  ........         "
    ".......  "
    ".........    ..  ::^5GBG5?!~^.7Y::7YJ!!~:^^~!^.7^^:^^^^^:..:.^          "
    ".....   "
    ".................^: YBY77^::^^   :^~7JYY?77?JY^:?7!!~~~~:.:^::           "
    "....   "
    "................ YPJJPGP5J!!J!~G7..^??JJJY555Y!^?777!~~!!.:^^.         "
    "........ "
    "....::::.........7##BGGPYJ?JJ~##P!. :7J555YYJ?^!77!!!!!7!^^~~.         "
    "........ "
    "...:::::::::......7GB#PYY??J?P&GY77!~:^!!!~~~^~~~!!!!77?77??: "
    ".................."
    "..:::::::::::......JPY?!~^!5J7J!^...:^^!!!!~~^^^^~7777??J5?^.............."
    "......"
    "^^^^~~~~~^^^^::::::7GPJ~!5BBGGGJ?77????7!~^~^^!!!!!!7!7?7^................"
    "......"
    "77777???7777!!!!!!!7P5!!YGB###BGP5J77!~:.:^!!??"
    "!~~~~~~~7!................."
    "......"
    "YYYYYYYYYYYYYYYYYYYJ5GP55YJ!^~!77~!^::::^~!7??"
    "!~~^^^^^^~~^::::::::........"
    "......"
    "PPPPPPPPPPPPPPPPPPPPPPGPP5GBGPPPP5J?77777777!~~^:::::::^7?777!!!~~~~^^^^^:"
    "::::::"
    "BBBBBBBBBBBBBBBBBBBBBBG55PB###BPYJ??777777?!^:::::::::!G77Y55YYYJJJJ??"
    "7777!!!~~~"
    "BBBB#BBBB##############GY5B####BGPPP5YJJ?7~^:::::.:::?#&G.."
    "7GGPPPPPP5555YYYJJJJJ"
    "B########################PYG####GPP5J?7!~^:.:::::.:~5#&&&7 "
    ".~5BBBBGGGGGGGPPPP555"
    "B#########################B5555Y?!!~^^^::::::::.:~JG##&&G. "
    "...!P####BBBBBBBGGGGG"
    "## TOO YOUNG TOO SIMPLE ###&PJ?7!^:::::::^::::~?5GB####G.  "
    ".....~?YPGB###BBBBBBB"
    "#### SOMETIMES NAIVE ##BP5J#@BPY?7~^^^^^^:.^?5GB##BB#&#:    "
    "........:^!J5PBBBBBB";

void print_toad() {
    pts_t* pts = get_current()->pts;
    pts_set_cursor(pts, 0, 0);
    pts_set_term_color(pts, FGND_WHITE | BGND_BLACK);
    pts_putbytes(pts, toad, sizeof(toad) - 1);
}
