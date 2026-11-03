script "community.ferruslogic.library.photonjson"
/*
Code repository: https://github.com/DTByte-Ltd/PhotonJSON
Web: https://dtbyte.com/
License: https://opensource.org/licenses/MIT
Date: Nov 2020
Last Update: October 19, 2025
Note: PhotonJSON follows JSON's standards presented
in https://www.json.org/json-en.html
*/


on extensionInitialize
   if the target is not me then
      pass extensionInitialize
   end if

   if the environment contains "development" then
      set the _ideOverride of me to true
   end if

   insert the script of me into back
end extensionInitialize


on extensionFinalize
   if the target is not me then pass extensionFinalize
   remove the script of me from back
end extensionFinalize

/**
Type: library
Title: PhotonJSON
Version: 1.2.5
Author: DTByte-Ltd
SVGIcon: M173.17275,101.24755c-0.31882,-0.37688 -0.63584,-0.75556 -0.97189,-1.15557c0.11837,-0.24671 0.14286,-0.33878 0.19637,
-0.40817c0.28617,-0.35193 0.57869,-0.69842 0.87121,-1.04491c2.99006,-3.53791 5.74791,-7.24499 8.1747,-11.19333c2.57237,
-4.18554 4.75153,-8.56019 6.24089,-13.25277c1.54968,-4.88305 2.38824,-9.85227 1.81499,-15.00472c-0.66078,-5.93613 -3.11478,
-10.92848 -7.6264,-14.87682c-2.6218,-2.29436 -5.60233,-3.99052 -8.82414,-5.28578c-4.84677,-1.94787 -9.91577,-2.8862 -15.09995,
-3.18961c-5.81231,-0.34014 -11.60105,-0.00544 -17.35214,0.92246c-1.23902,0.2 -2.47849,0.39547 -3.73973,0.59683c0.02041,
-0.11701 0.01179,-0.18141 0.03855,-0.20318c0.31111,-0.2576 0.62495,-0.51202 0.94287,-0.76146c3.45446,-2.70842 4.85357,
-7.17333 3.55061,-11.33846c-1.71975,-5.49757 -7.44181,-8.53706 -12.98655,-6.90076c-0.11655,0.03447 -0.23084,0.07937 -0.34604,
0.12517c-0.30114,0.11882 -0.60908,0.24037 -0.97734,0.17823c-0.28118,-0.4127 -0.56055,-0.82223 -0.83992,-1.23221c-2.46125,
-3.6132 -5.24587,-6.94521 -8.56019,-9.81418c-3.00865,-2.60457 -6.31844,-4.71344 -10.08402,-6.05722c-3.84177,-1.37054 -7.76971,
-1.72338 -11.78336,-0.94378c-2.87668,0.55874 -5.55517,1.6431 -8.08627,3.10299c-2.72656,1.57281 -5.17013,3.51478 -7.40825,
5.72161c-3.57691,3.52748 -6.5003,7.55202 -9.0246,11.87951c-2.72611,4.67443 -4.90981,9.5983 -6.74793,14.6809c-0.19683,
0.54468 -0.4118,1.08255 -0.62994,1.65353c-0.43991,-0.04172 -0.79683,-0.06032 -1.14877,-0.11202c-2.42044,-0.35601 -4.83407,
-0.7669 -7.26086,-1.06895c-5.14564,-0.63992 -10.30534,-0.72745 -15.47366,-0.25941c-3.44267,0.31157 -6.82321,0.93607 -10.12438,
1.96102c-4.06309,1.26124 -7.84272,3.08076 -11.15478,5.78964c-5.20868,4.26037 -7.91121,9.80148 -8.36972,16.47412c-0.29887,4.3479 0.30477,8.58967 1.53154,
12.75798c1.4431,4.90346 3.61592,9.47585 6.24134,13.84416c2.57599,4.28622 5.5529,8.28128 8.78378,12.09266c0.2839,0.33515 0.54831,0.68708 0.88436,1.11022c-0.09161,
0.01134 -0.16826,0.02676 -0.23311,0.03946c-0.11474,0.02268 -0.19365,0.0381 -0.25805,0.01451c-0.33878,-0.12563 -0.66214,-0.29388 -0.99729,-0.4313c-5.11389,
-2.09617 -10.97383,0.14422 -13.36978,5.11027c-1.85762,3.85039 -1.15239,8.50713 1.75149,11.6532c0.32971,0.35737 0.79593,0.61225 0.94786,1.17054c-0.16236,
0.34966 -0.34059,0.71429 -0.50295,1.08573c-0.63538,1.45263 -1.31612,2.88802 -1.88982,4.36513c-1.66805,4.29348 -2.78597,8.71484 -2.93972,13.34574c-0.08753,
2.64085 0.09479,5.25494 0.73244,7.82776c1.34514,5.42773 4.40096,9.68039 8.88582,12.95344c2.77146,2.0227 5.82411,3.49437 9.06814,
4.57783c3.54063,1.18278 7.18149,1.87576 10.89492,2.24175c3.93928,0.38867 7.88173,0.37098 11.821,0.11746c3.75605,-0.24173 7.4858,-0.74151 11.18335,
-1.46442c0.04308,-0.00816 0.08617,-0.01769 0.12971,-0.02721c0.30522,-0.06576 0.62767,-0.1356 0.95693,0.11882c0.17188,0.46894 0.361,0.97824 0.54558,
1.48891c1.99186,5.51345 4.39053,10.83641 7.44453,15.84917c2.65536,4.35833 5.73295,8.38196 9.51939,11.83007c2.97419,2.70842 6.2554,4.94156 10.01735,
6.42729c5.66039,2.2354 11.31261,2.22678 16.9671,-0.02222c2.33064,-0.927 4.49302,-2.15286 6.52616,-3.61365c2.92747,-2.10297 5.48578,-4.59915 7.79012,
-7.36199c3.77193,-4.5225 6.72888,-9.55159 9.22143,-14.86821c1.30886,-2.79232 2.45037,-5.65358 3.50798,-8.54931c0.01179,-0.03175 0.02132,-0.06576 0.03129,-0.10023c0.05079,
-0.17687 0.10476,-0.36644 0.43493,-0.33515c0.03175,0.29615 0.06304,0.60046 0.0966,0.90432c0.67937,6.13658 6.7316,10.39559 12.72668,8.89761c4.09801,-1.02405 6.68353,-3.64041 7.77878,
-7.723c0.11429,-0.4254 0.22359,-0.85216 0.33787,-1.28936c0.43447,-0.03628 0.79185,-0.06894 1.15013,-0.09615c3.75968,-0.28617 7.47764,-0.82223 11.10716,
-1.87667c4.12703,-1.19956 7.99375,-2.93654 11.41646,-5.57739c3.57011,-2.75514 6.21005,-6.19599 7.68898,-10.48493c0.84944,-2.46216 1.25897,
-5.00732 1.35965,-7.60191c0.11701,-3.00911 -0.26032,-5.97195 -0.91566,-8.90351c-1.09661,-4.90437 -3.01319,-9.49172 -5.41004,-13.88906c-2.85446,
-5.23725 -6.32661,-10.04547 -10.17382,-14.59111zM122.998,21.35452c0.04671,0.07302 0.05397,0.17052 0.11066,0.35964c-4.75969,6.68262 -1.12564,
15.6274 7.33342,16.91313c-0.27574,0.14014 -0.45035,0.27347 -0.64581,0.32245c-10.09491,2.53246 -19.78074,6.20506 -29.25115,10.47994c-0.16417,
0.07392 -0.33334,0.13696 -0.58277,0.23901c-9.98289,-4.53838 -20.26011,-8.41507 -30.98541,-11.02099c0,-0.08345 -0.00454,-0.1551 -0.00862,
-0.2186c-0.00726,-0.11429 -0.01315,-0.20046 0.01361,-0.27529c2.2286,-6.30121 4.98328,-12.34845 8.71303,-17.91858c2.1515,-3.21364 4.59597,
-6.18103 7.49261,-8.75884c2.11023,-1.87803 4.40413,-3.4853 6.97559,-4.67625c2.88756,-1.33743 5.90211,-2.0227 9.10034,-1.78279c2.05535,
0.1542 4.01093,0.69615 5.89984,1.49616c2.22678,0.94332 4.25765,2.2159 6.14384,3.72023c3.90708,3.11568 7.02049,6.92571 9.69082,11.12077zM61.12379,
42.07314c0.3932,0.0576 0.77507,0.19637 1.24582,0.31928c-1.54786,5.33975 -2.78053,10.67133 -3.74834,16.06187c-0.96917,
5.3978 -1.66397,10.8355 -2.21227,16.30088c-2.93926,2.23676 -5.90075,4.40005 -8.75884,6.69214c-2.86307,2.29617 -5.68079,
4.65765 -8.4128,7.10802c-2.72883,2.44719 -5.32659,5.03997 -8.02413,7.60916c-0.15374,-0.14377 -0.3288,-0.27891 -0.46894,
-0.444c-4.39189,-5.1656 -8.28264,-10.66679 -11.22462,-16.8002c-1.75195,-3.6522 -3.10888,-7.43864 -3.8005,-11.44322c-0.62359,
-3.61183 -0.68345,-7.21551 0.24445,-10.79106c0.87983,-3.39052 2.67668,-6.22003 5.29439,-8.53661c2.05762,-1.82134 4.39234,
-3.18961 6.91346,-4.26218c3.19142,-1.35784 6.52525,-2.18007 9.94887,-2.66307c5.51072,-0.77733 11.0287,-0.71429 16.54351,
-0.05714c2.15876,0.25715 4.30844,0.59048 6.45994,0.90613zM140.28438,53.13586c-0.81724,-3.56694 -1.70115,-7.11845 -2.57146,
-10.74072c0.30794,-0.09977 0.59502,-0.23266 0.89616,-0.2839c3.08258,-0.522 6.17468,-0.9601 9.29535,-1.20772c2.76148,
-0.21905 5.52297,-0.30159 8.28672,-0.18685c4.91344,0.20408 9.73799,0.92518 14.36797,2.67078c2.77554,1.04627 5.35925,
2.42951 7.65406,4.32885c3.87397,3.20684 5.91617,7.34793 6.38103,12.32577c0.42132,4.51026 -0.38005,8.84773 -1.78687,
13.09948c-1.34423,4.06309 -3.24902,7.86495 -5.4758,11.51125c-2.34379,3.83814 -5.01729,7.43592 -7.92708,10.8609c-0.19819,
0.23311 -0.43039,0.43765 -0.71792,0.72699c-7.67946,-8.04091 -16.17707,-15.01243 -25.0393,-21.34039c-0.49071,-3.79596 -0.93879,
-7.43592 -1.43675,-11.06907c-0.49207,-3.59097 -1.11657,-7.16199 -1.9261,-10.69537zM92.7287,51.91226c0.36962,0.15374 0.72155,
0.34921 1.15149,0.5601c-0.22721,0.16599 -0.35828,0.29479 -0.51429,0.37189c-10.8219,5.31118 -21.13087,11.51397 -31.29425,
17.97074c-0.01723,0.01088 -0.04581,0.00363 -0.09705,-0.00952c-0.03311,-0.00816 -0.07528,-0.01905 -0.12971,-0.02903c1.06849,
-9.22007 2.72792,-18.32267 5.40551,-27.29919c0.30885,0.02676 0.57189,0.01633 0.8186,0.07574c6.87673,1.65852 13.5657,
3.90209 20.13811,6.50303c1.51476,0.59955 3.01772,1.22949 4.5216,1.85626zM110.87042,55.01252c-1.5592,-0.81362 -3.12748,
-1.60954 -4.73793,-2.43676c2.99414,-2.06669 22.2565,-8.65634 26.53365,-9.08945c0.12744,0.35329 0.28526,0.71974 0.39366,
1.10024c1.50342,5.27217 2.72021,10.61101 3.60957,16.0206c0.46259,2.81364 0.85897,5.63862 1.2685,8.46133c0.00952,
0.06576 0.01995,0.13197 0.02993,0.19864c0.0771,0.50069 0.15737,1.01997 -0.00136,1.63222c-1.60592,-0.89434 -3.0209,
-1.93517 -4.50844,-2.85672c-1.46306,-0.90659 -2.91205,-1.83585 -4.37919,-2.73518c-1.50569,-0.92246 -3.0209,-1.83086 -4.54291,
-2.72702c-1.48301,-0.87303 -2.97373,-1.73335 -4.47171,-2.58008c-1.53743,-0.86894 -3.08167,-1.7261 -4.63407,-2.56828c-1.51204,
-0.82042 -3.0345,-1.6236 -4.55969,-2.41953zM99.65985,55.33679c0.1229,-0.05215 0.26168,-0.06667 0.46849,-0.11655c13.44098,
6.39962 26.33773,13.84234 38.6948,22.35673c1.19457,14.9571 1.19457,29.89016 0,44.84952c-12.35434,8.51257 -25.25019,
15.95529 -38.78051,22.39846c-0.42359,-0.18504 -0.84173,-0.35148 -1.24627,-0.54604c-12.68405,-6.10574 -24.84882,-13.11218 -36.473,
-21.05468c-1.28346,-0.87711 -1.17416,-0.64218 -1.29616,-2.29254c-0.55103,-7.46449 -0.81815,-14.9385 -0.78686,-22.4234c0.02313,
-5.48805 0.26304,-10.96839 0.56554,-16.44691c0.06485,-1.17643 0.19048,-2.35014 0.24626,-3.52703c0.03039,-0.64037 0.25442,
-1.09752 0.8118,-1.45399c1.22178,-0.78187 2.40275,-1.62678 3.61274,-2.42679c10.64049,-7.0341 21.69459,-13.34211 33.19496,
-18.86372c0.32653,-0.15692 0.65488,-0.31111 0.98822,-0.45307zM144.47263,88.38707c-0.0712,-2.21227 -0.22721,-4.42182 -0.35239,
-6.79192c3.62045,1.87531 18.05146,14.4614 21.13177,18.37936c-0.14286,0.47801 -0.56917,0.77098 -0.90387,1.11838c-4.37738,
4.54019 -9.0636,8.74206 -13.98067,12.68723c-1.73109,1.38913 -3.49982,2.73065 -5.25585,4.08848c-0.24127,0.1864 -0.50704,0.3415 -0.72518,
0.48708c-0.11565,-0.05805 -0.16281,-0.06939 -0.19002,-0.09887c-0.02857,-0.03175 -0.05669,-0.08163 -0.05442,-0.122c0.60182,
-9.91032 0.64989,-19.82745 0.33062,-29.74775zM35.62461,101.1006c-0.31202,-0.32381 -0.61271,-0.65896 -0.97099,-1.04491c3.05763,
-3.92068 17.40928,-16.44555 21.00071,-18.36122c0.24082,0.1415 0.21225,0.35601 0.18504,0.561c-0.00726,0.05261 -0.01406,
0.10476 -0.01633,0.15465c-0.09796,2.21635 -0.23719,4.4318 -0.30794,6.64906c-0.0966,3.03224 -0.16826,6.06583 -0.20862,
9.09943c-0.07166,5.39054 0.07211,10.77836 0.31882,16.16301c0.05986,1.30795 0.15828,2.61454 0.23764,3.90572c-0.40998,
0.15737 -0.63856,-0.04581 -0.8508,-0.23402c-0.05034,-0.0449 -0.10023,-0.08934 -0.15102,-0.12744c-3.66263,-2.7438 -7.2273,
-5.60868 -10.6677,-8.62686c-2.96375,-2.60003 -5.83136,-5.30074 -8.56881,-8.13842zM167.75183,104.72196c0.29025,
-0.2839 0.54558,-0.60273 0.89706,-0.99502c0.283,0.26531 0.42495,0.37461 0.53969,0.50794c4.21728,4.94881 7.96835,
10.21509 10.87677,16.05008c1.84673,3.70481 3.29891,7.55111 4.07669,11.63007c0.70296,3.68667 0.83538,7.37016 -0.05079,
11.05546c-0.86668,3.60231 -2.73019,6.58149 -5.51118,9.0051c-2.49436,2.17418 -5.3656,3.69392 -8.45861,4.80686c-3.47124,
1.24899 -7.06176,1.97145 -10.73482,2.27304c-0.27937,0.02313 -0.55874,0.05578 -0.83992,0.08889c-0.75375,0.08798 -1.51657,
0.17778 -2.31613,0.08254c-0.1737,-0.31202 -0.35057,-0.62087 -0.51928,-0.93471c-3.47306,-6.46221 -12.03688,-7.53887 -17.01472,
-2.14016c-0.22721,0.24626 -0.46849,0.47937 -0.77915,0.67484c2.86217,-10.35069 4.60459,-20.87191 5.63907,
-31.6158c0.39139,-0.29706 0.81362,-0.63039 1.24899,-0.94514c5.95653,-4.30255 11.70581,-8.86315 17.09499,
-13.86275c1.99231,-1.84855 3.90708,-3.78054 5.85132,-5.68125zM19.75594,120.58746c0.15692,-0.32019 0.34921,
-0.62314 0.54695,-0.9728c4.13746,0.53787 7.68717,-0.56463 10.29037,-3.90163c2.60321,-3.33609 2.78371,
-7.04816 1.2685,-10.89129c0.28254,-0.19139 0.41497,-0.04082 0.52926,0.08844c0.02086,0.02404 0.04172,0.04762 0.06259,
0.06803c1.20001,1.17235 2.37327,2.37146 3.58598,3.5302c5.87717,5.61594 12.17838,10.72076 18.7585,15.48635c0.44082,
0.31928 0.87938,0.64037 1.31657,0.96373c0.07166,0.05352 0.13107,0.12381 0.28345,0.26939c0.5601,5.4046 1.24355,
10.88766 2.2227,16.32809c0.96963,5.38737 2.20456,10.71578 3.7302,15.99611c-0.81679,0.45035 -1.65444,0.45352 -2.44266,
0.58368c-5.42365,0.89616 -10.88176,1.39548 -16.38342,1.14332c-5.09802,-0.23356 -10.09445,-1.02677 -14.86004,-2.95423c-2.10932,
-0.85307 -4.09892,-1.92293 -5.93613,-3.27396c-4.82682,-3.54925 -7.19328,-8.37198 -7.44091,-14.30267c-0.16463,
-3.94699 0.54468,-7.76064 1.74469,-11.4981c0.73561,-2.29028 1.66623,-4.503 2.72339,-6.66267zM93.86341,147.43948c-3.37374,
2.1837 -22.22702,8.62323 -26.45837,9.06723c-1.58142,-2.94879 -5.62546,-23.18486 -5.45086,-27.37675c1.16328,0.47075 2.08529,
1.27802 3.11206,1.90388c1.04355,0.63584 2.0617,1.31385 3.09664,1.96465c1.03493,0.65035 2.07349,1.29525 3.11659,
1.93245c1.0431,0.6372 2.09073,1.26714 3.14199,1.89118c1.09026,0.64672 2.18415,1.28754 3.28167,1.92157c1.05852,
0.61135 2.12111,1.21543 3.18689,1.81408c1.06532,0.59819 2.13427,1.19185 3.20729,1.77689c1.07303,0.58504 2.15014,
1.16283 3.23133,1.7329c1.08119,0.57007 2.16919,1.12745 3.25537,1.68755c1.07575,0.55466 2.15331,1.10614 3.2794,
1.68437zM137.87754,129.20843c0.03311,0.00952 0.07574,0.02132 0.13107,0.03311c0.08118,4.49302 -3.71342,
23.61888 -5.39689,27.25566c-3.69755,-0.24173 -23.08871,-6.84679 -26.36993,-8.98197c0.01587,-0.03674 0.02676,
-0.07664 0.04853,-0.1093c0.02449,-0.03674 0.05442,-0.07302 0.0898,-0.09887c0.07302,-0.05306 0.14739,
-0.10658 0.22812,-0.14649c7.26358,-3.60866 14.36162,-7.51936 21.28053,-11.7498c3.32204,-2.03086 6.59418,
-4.14291 9.89128,-6.21459c0.01678,-0.01043 0.04535,-0.00227 0.09751,0.01225zM100.01632,150.33294c10.00239,
4.55697 20.24832,8.39965 31.05706,11.04911c-0.09796,0.40545 -0.13016,0.62631 -0.20454,0.8313c-2.06216,
5.72887 -4.55198,11.2541 -7.83547,16.39929c-1.98188,3.1048 -4.21592,6.0105 -6.84407,8.60056c-2.01046,
1.98143 -4.20141,3.73066 -6.67174,5.11253c-2.02996,1.13561 -4.16241,1.9905 -6.4613,2.4159c-3.06126,
0.56599 -6.0418,0.27755 -8.96609,-0.75647c-2.63405,-0.93153 -5.00505,-2.32973 -7.19918,-4.04086c-3.06217,
-2.38824 -5.64043,-5.2377 -7.9239,-8.36427c-3.36058,-4.60187 -5.96288,-9.62052 -8.13026,-14.87365c-0.62132,
-1.50659 -1.17235,-3.04267 -1.74877,-4.56786c-0.07619,-0.20136 -0.11111,-0.41815 -0.18594,-0.71021c2.19776,
-0.61044 4.33202,-1.17326 6.44815,-1.79911c2.08438,-0.61633 4.1615,-1.2626 6.22094,-1.95739c2.10207,
-0.70931 4.19053,-1.46487 6.26447,-2.25354c2.03177,-0.77234 4.04812,-1.58959 6.05359,-2.4286c2.04492,
-0.85534 4.06898,-1.76193 6.12706,-2.65672z
OS: mac, windows, linux, ios, android
Platforms: desktop, mobile, web
Description:
LiveCode script library for working with JSON, written in livecode-script and without the use of external.
*/


/* Declare script local variables */
local sIndentSize, sMaxLength

/* This flag is used to determine if the function is being called by itself.
So you don't have to repeat some operations and only do them when you finish
building the JSON. */
local sRecursionCounter = 0


/* These are the four functions that are optionally included for PhotonJSON
compatibility with projects that use mergeJSON or FastJSON.
They expect the same parameters as in their original versions
and in the same order.

Warning! These functions will override original ones, if you don't want
this behavior then comment out these four functions */


/**
Summary: Format a LC array into UTF-8 encoded json data.

Parameters:
pArray (Array): The array to be formatted
pForceRootType (String): The type to force values to.
pPretty (Boolean): Set to true to format the resulting JSON data nicely.

Returns (String): JSON data.

Synonyms: JSONStringify

Description:
Format a LC array into UTF-8 encoded json data.
*/
function ArrayToJSON pArray, pForceRootType, pPretty
   return JSONStringify(pArray, pForceRootType, pPretty)
end ArrayToJSON


/**
Summary: Parse JSON text into a LiveCode array.

Parameters:
pJsonString (string): String containing UTF-8 encoded JSON text

Returns: A LiveCode array (Array).

Synonyms: JSONParser, arrayFromJson

Description:
<JSONParser> is used to convert data encoded in JSON format into a LiveCode
array. It parses <pJsonString> and returns resulting array.
*/
function JSONToArray pJsonString
   return JSONParser(pJsonString)
end JSONToArray


/**
Summary: Parse JSON text into a LiveCode array.

Parameters:
pJsonString (string): String containing UTF-8 encoded JSON text

Returns: A LiveCode array (Array).

Synonyms: JSONParser, JSONToArray

Description:
<arrayFromJson> is used to convert data encoded in JSON format into a LiveCode
array. It parses <pJsonString> and returns resulting array.
*/
function arrayFromJson pJsonString
   return JSONParser(pJsonString)
end arrayFromJson


/**
Summary: Format a LC array into UTF-8 encoded json data.

Parameters:
pArray (Array): The array to be formatted

Returns (String): JSON data.

Description:
Format a LC array into UTF-8 encoded json data.
*/
function jsonFromArray pArray
   return JSONStringify(pArray)
end jsonFromArray


/* CORE DECLARATIONS */


/**
Summary: Validate if a string is a valid JSON.

Parameters:
pJsonString (String): The UTF-8 encoded JSON data to be verified.

Returns (Boolean): A Boolean.

Description:
Validate if a string is a valid JSON.
*/
function validateJSON pJsonString
   local tCont = 0, tTokenCont = 0, tSplit

   if _validateJSON(pJsonString) is false then
      return false
   end if

   /* escaped quotes <" > and reverse-solidus < \\ > will confuse the tokenizer
   so we replace them with "\u0022" and "\u005C" which we'll unescape
   with all the other unescapes when we parse */

   put replaceText(pJsonString, "\\\\", "\u005C") into pJsonString
   put replaceText(pJsonString, "\\" & quote, "\u0022") into pJsonString
   put replaceText(pJsonString, "\\/", "/") into pJsonString

   put _cleaningNonPrintableChars(pJsonString) into pJsonString

   repeat for each token tToken in pJsonString
      add 1 to tTokenCont

      switch tToken
         case "{"; case "["
            add 1 to tCont
            break

         case "}"; case "]"
            subtract 1 from tCont
            break

         case "+"
            put the token (tTokenCont - 1) to (tTokenCont + 1) of pJsonString \
                  into tSplit

            split tSplit by quote

            if tSplit[2] is not "+" then
               return false
            end if
            break

      end switch

      if tToken begins with "0" and tToken is not 0 then
         put the token (tTokenCont - 1) to (tTokenCont + 1) of pJsonString \
               into tSplit

         split tSplit by quote

         if tSplit[2] is not tToken then
            return false
         end if
      end if
   end repeat

   return (tCont is 0)
end validateJSON


/**
Summary: JSON string formatted in a more visually pleasing way.

Parameters:
pJsonString (String): The UTF-8 encoded JSON data to be beautify.

Returns (String): UTF-8 JSON string.

Description:
JSON string formatted in a more visually pleasing way.
*/
function beautifyJSON pJsonString, pIndentSize
   if pIndentSize is an integer then
      put pIndentSize into sIndentSize
   else
      put 2 into sIndentSize
   end if

   replace tab with empty in pJsonString
   replace CR with empty in pJsonString
   put _cleaningNonPrintableChars(pJsonString) into pJsonString

   return _jsonPrettyCompact(pJsonString, pIndentSize)
end beautifyJSON


/**
Summary: Remove unnecessary spaces from JSON string.

Parameters:
pJsonString (String): The UTF-8 encoded JSON data to be minified.

Returns (String): Minified JSON data.

Description:
Remove unnecessary characters from JSON data.
*/
function minifyJSON pJsonString
   local tFormatted, tPrevToken = "", tIndex = 0, tOpenClose = ""
   local tItems = "", tHyphenMinus = false

   if _validateJSON(pJsonString) is false then return pJsonString
   replace tab with empty in pJsonString
   replace CR with empty in pJsonString

   # escaped quotes < " > and reverse-solidus < \\ > will confuse the tokenizer
   # so we replace them with "\u0022" and "\u005C" which we'll unescape
   # with all the other unescapes when we parse
   put replaceText(pJsonString, "\\\\", "\u005C") into pJsonString
   put replaceText(pJsonString, "\\" & quote, "\u0022") into pJsonString
   put replaceText(pJsonString, "\\/", "/") into pJsonString
   put _cleaningNonPrintableChars(pJsonString) into pJsonString

   repeat for each token tToken in pJsonString

      switch tToken

         case "{"; case "["; case "}"; case "]"; case ":"; case comma
            put tToken after tFormatted
            break

         case "-"
            put true into tHyphenMinus
            next repeat
            break

         default
            if tHyphenMinus then
               put "-" & tToken into tToken
               put false into tHyphenMinus
            end if

            # The token is still JSON-escaped text, so restore the placeholders
            # to their escape sequences and re-quote it without escaping again.
            replace "\u005C" with "\\" in tToken
            replace "\u0022" with "\" & quote in tToken

            put quote & tToken & quote after tFormatted
      end switch

      put tToken into tPrevToken
   end repeat

   put textDecode(tFormatted, "UTF8") into tFormatted

   return tFormatted
end minifyJSON



/**
Summary: Format a LC array into UTF-8 encoded json data.

Parameters:
pArray (Array): The array to be formatted
pForceRootType (String): The type to force values to.
pPretty (Boolean): Set to true to format the resulting JSON data nicely.

Returns (String): JSON data.

Description:
Format a LC array into UTF-8 encoded json data.
*/
function JSONStringify pArray, pForceRootType, pPretty
   local tJsonString = "[", tIsNumeric = true, tKeys = "", tEndChar = "]"

   add 1 to sRecursionCounter

   if pArray is not an array then return "{}"

   # Only an array whose keys are exactly 1 to n is written as a JSON list.
   # Any other set of integer keys (sparse, or not starting at 1) is written
   # as an object, otherwise the keys would be lost: {2: "x", 5: "y"} used to
   # come out as ["x","y"].
   if the extents of pArray is not ("1," & the number of elements in pArray) then
      put false into tIsNumeric
      put "{" into tJsonString
      put "}" into tEndChar
   end if
   
   set the caseSensitive to true
   
   local tOptions
   put pForceRootType into tOptions["string"]
   put pPretty into tOptions["pretty"]
   # The values of the JSON keys will be formatted to their type.
   _builderJSON pArray, tIsNumeric, tOptions, tJsonString
   
   put tEndChar into the char - 1 of tJsonString

   # Empty list and empty object: JSONParser returns them as an array with
   # one empty element, under the key 1 or "", so write those back as [] and {}
   if tJsonString is "[" & quote & quote & "]" then
      put "[]" into tJsonString
   else if tJsonString is "{" & quote & quote & ": " & quote & quote & "}" then
      put "{}" into tJsonString
   end if

   subtract 1 from sRecursionCounter

   if sRecursionCounter is 0 then
      set the caseSensitive to false
      if numToChar(8) is in tJsonString then replace numToChar(8) with "\b" in tJsonString
      if numToChar(9) is in tJsonString then replace numToChar(9) with "\t" in tJsonString
      if numToChar(10) is in tJsonString then replace numToChar(10) with "\n" in tJsonString
      if numToChar(12) is in tJsonString then replace numToChar(12) with "\f" in tJsonString
      if numToChar(13) is in tJsonString then replace numToChar(13) with "\r" in tJsonString
      // non-printable characters to your unicode code (HEX)
      put _cleaningNonPrintableChars(tJsonString) into tJsonString

      # Pretty
      if pPretty then return _jsonPrettyCompact(tJsonString, 2, pForceRootType)

      # tJsonString holds the UTF-8 bytes of the document one native character
      # per byte (see _jsonString); hand them back as binary data
      return textEncode(tJsonString, "native")
   end if

   return tJsonString
end JSONStringify


/**
Summary: Parse JSON text into a LiveCode array.

Parameters:
pJsonString (string): String containing UTF-8 encoded JSON text

Returns: A LiveCode array (Array).

Synonyms: JSONToArray, arrayFromJson

Description:
<JSONParser> is used to convert data encoded in JSON format into a LiveCode
array. It parses <pJsonString> and returns resulting array.
*/
function JSONParser pJsonString
   local tKeyNameOrValue = 0, tArrayKeysPath, tNumKeys = 0, tOpenJArray, tResultArray, tHyphenMinus, tToken, tOldToken, tObjEmpty
   local tLastValue, tExponentSign

   if pJsonString is empty then return empty
   put _jsonTextFromInput(pJsonString) into pJsonString

   # replaceText rather than replace throughout the pre-pass: on text outside
   # ASCII, replace costs about 100 ms per call on a megabyte of text on this
   # engine, replaceText a few milliseconds.
   put replaceText(pJsonString, "[\t\n]", "") into pJsonString

   # escaped quotes < " > and reverse-solidus < \\ > will confuse the tokenizer
   # so we replace them with "\u0022" and "\u005C" which we'll unescape
   # with all the other unescapes when we parse
   put replaceText(pJsonString, "\\\\", "\u005C") into pJsonString
   put replaceText(pJsonString, "\\" & quote, "\u0022") into pJsonString
   put replaceText(pJsonString, "\x{0}", "\u0000") into pJsonString

   # the token chunk drops the quotes, so hide a quoted "null" from the bare
   # null check below; the escape is undone with the others
   put replaceText(pJsonString, quote & "null" & quote, quote & "\u006Eull" & quote) into pJsonString

   if the number of tokens of pJsonString is 2 then
      if token 1 of pJsonString is "[" and the token 2 of pJsonString is "]" then
         put empty into tResultArray[1]
         return tResultArray
      else if token 1 of pJsonString is "{" and the token 2 of pJsonString is "}" then
         put empty into tResultArray[empty]
         return tResultArray
      end if
      return empty
   end if

   repeat for each token tToken in pJsonString
      if tNumKeys < 0 then put 0 into tNumKeys

      switch tToken
         case "{"
            put 0 into tKeyNameOrValue
            put tToken into tOldToken
            next repeat

         case "["
            add 1 to tNumKeys
            put 1 into tKeyNameOrValue
            put 1 into tArrayKeysPath[tNumKeys]
            put tNumKeys into tOpenJArray[tNumKeys]
            put tToken into tOldToken
            next repeat

         case "}"
            # Fix bug when JSON has empty objects
            if tOldToken is "{" then
               put empty into tObjEmpty
               put empty into tObjEmpty[empty]
               _builderArray tObjEmpty, tArrayKeysPath, tResultArray, tNumKeys
               put tToken into tOldToken
               next repeat
            end if

            delete variable tArrayKeysPath[tNumKeys]
            subtract 1 from tNumKeys
            put tToken into tOldToken
            next repeat

         case comma
            if tOpenJArray[tNumKeys] <> empty then
               if tArrayKeysPath[tNumKeys] is an integer then
                  add 1 to tArrayKeysPath[tNumKeys]
                  put 1 into tKeyNameOrValue
               end if
            else
               delete variable tArrayKeysPath[tNumKeys]
               subtract 1 from tNumKeys
               put 0 into tKeyNameOrValue
            end if

            put tToken into tOldToken
            next repeat

         case colon
            put 1 into tKeyNameOrValue
            put tToken into tOldToken
            next repeat

         case "]"
            delete variable tArrayKeysPath[tNumKeys]
            delete variable tOpenJArray[tNumKeys]
            subtract 1 from tNumKeys
            put 0 into tKeyNameOrValue

            # Fix bug when JSON has empty array
            if tOldToken is "[" then
               put empty into tObjEmpty
               put empty into tObjEmpty[1]
               _builderArray tObjEmpty, tArrayKeysPath, tResultArray, tNumKeys
               put tToken into tOldToken
            end if

            put tToken into tOldToken
            next repeat

         case "-"; case "+"
            # The token chunk splits "1e-5" into "1e", "-" and "5": a sign right
            # after a value that ends in "e" belongs to that number's exponent.
            if tOldToken is empty and matchText(tLastValue, "^-?[0-9.]+[eE]\z") then
               put tToken into tExponentSign
               next repeat
            end if
            if tToken is "-" then put true into tHyphenMinus
            put tToken into tOldToken
            next repeat

         case "null"
            # a bare null becomes an empty value
            if tKeyNameOrValue = 1 then put empty into tToken
            break
      end switch

      if backslash is in tToken then
         put _unescapeJSONString(tToken) into tToken
      end if

      if tOldToken is not in "{[,:-" then
         # JSON string format error
         return empty
      end if

      put empty into tOldToken

      if tKeyNameOrValue = 1 then
         if tExponentSign is not empty then
            # completes the number stored on the previous pass
            put tLastValue & tExponentSign & tToken into tToken
            put empty into tExponentSign
         else if tHyphenMinus then
            put "-" & tToken into tToken
            put false into tHyphenMinus
         end if

         put tToken into tLastValue
         _builderArray tToken, tArrayKeysPath, tResultArray, tNumKeys
      else if tKeyNameOrValue = 0 then
         add 1 to tNumKeys
         put tToken into tArrayKeysPath[tNumKeys]
      end if
   end repeat

   if tOldToken is in "{[,:-" then
      # JSON string format error
      return empty
   end if

   return tResultArray
end JSONParser

private command _builderJSON pArray, pIsNumeric, pOptions, @xJsonString
   local tString, tPretty
   if pOptions["string"] is "string" then
      put "string" into tString
   else
      put "" into tString
   end if

   put pOptions["pretty"] is true into tPretty

   local tValue
   if pIsNumeric then
      repeat for each element e in pArray
         if e is an array then
            put JSONStringify(e, tString, tPretty) & comma after xJsonString
         else 
            put _jsonValue(e) into tValue
            if tString is "string" then
               put quote & tValue & quote into tValue
            end if
            put tValue & comma after xJsonString
         end if
      end repeat
   else
      # Keys are written in sorted order so that the same data always gives
      # the same text, which is what makes the output usable with diff and
      # version control. LiveCode's own key order varies between runs.
      local tKeys
      put the keys of pArray into tKeys
      sort lines of tKeys international
      if tKeys is empty and the number of elements in pArray is 1 then
         # the only key is the empty string, which "the keys" gives as empty
         # text; one empty line makes the loop below visit it
         put return into tKeys
      end if
      repeat for each line k in tKeys
         if pArray[k] is an array then
            put _jsonString(k) & ": " &JSONStringify(pArray[k], tString, tPretty) & comma after xJsonString
         else
            put _jsonValue(pArray[k]) into tValue
            if tString is "string" then
               put quote & tValue & quote into tValue
            end if            
            put _jsonString(k) & ": " & tValue & comma after xJsonString
         end if
      end repeat
   end if
end _builderJSON

private function _jsonString pValue
   if pValue is not an ascii string then put textEncode(pValue, "UTF-8") into pValue

   # So these characters escape in case they are there.
   # https://www.json.org/img/string.png
   # any other value is processed as a string.
   # JSON strings require that certain characters be 'escaped' with a backslash:
   # This makes the processing a bit slower. But it is necessary for it to comply with the ECMA-404 standard
   if "\" is in pValue then replace "\" with "\\" in pValue
   if quote is in pValue then replace quote with "\" & quote in pValue
   return quote & pValue & quote
end _jsonString


private function _jsonValue pValue, pNoReplace
   switch pValue
      case "true"
         return "true"
         break

      case "false"
         return "false"
         break

      case ""
         return quote & quote
         break

      case "null"; case null
         return "null"
         break

      default
         # Write a value bare only when it already is a number in its canonical
         # form: an optional minus, no leading zeros, no trailing zeros in the
         # fraction, no exponent. "007", "3.10" and "1e5" are text that happens
         # to evaluate as a number, and are written as strings. \z rather than
         # $, which in a regex also matches before a final newline: "6" & LF
         # is a string.
         if matchText(pValue, "^-?(0|[1-9][0-9]*)(\.[0-9]*[1-9])?\z") then
            return pValue
         end if

         if pNoReplace is no true then
            return _jsonString(pValue)
         else
            return quote & pValue & quote
         end if
   end switch
end _jsonValue


private command _builderArray @pValue, @pListKeys, @pArray, @pNumKeys
   local tArray

   # Adding caseSensitive when creating key names, to avoid overwriting keys such as the following:
   # A100 = "50", a100 = 33
   set the caseSensitive to true

   switch pNumKeys
      case 0
         -- break
      case 1
         put pValue into pArray[pListKeys[1]]
         break
      case 2
         put pValue into pArray[pListKeys[1]][pListKeys[2]]
         break
      case 3
         put pValue into pArray[pListKeys[1]][pListKeys[2]][pListKeys[3]]
         break
      case 4
         put pValue into pArray[pListKeys[1]][pListKeys[2]][pListKeys[3]][pListKeys[4]]
         break
      case 5
         put pValue into pArray[pListKeys[1]][pListKeys[2]][pListKeys[3]][pListKeys[4]][pListKeys[5]]
         break
      case 6
         put pValue into pArray[pListKeys[1]][pListKeys[2]][pListKeys[3]][pListKeys[4]][pListKeys[5]][pListKeys[6]]
         break
      case 7
         put pValue into pArray[pListKeys[1]][pListKeys[2]][pListKeys[3]][pListKeys[4]][pListKeys[5]][pListKeys[6]][pListKeys[7]]
         break
      case 8
         put pValue into pArray[pListKeys[1]][pListKeys[2]][pListKeys[3]][pListKeys[4]][pListKeys[5]][pListKeys[6]][pListKeys[7]][pListKeys[8]]
         break
      case 9
         put pValue into pArray[pListKeys[1]][pListKeys[2]][pListKeys[3]][pListKeys[4]][pListKeys[5]][pListKeys[6]][pListKeys[7]][pListKeys[8]][pListKeys[9]]
         break
      case 10
         put pValue into pArray[pListKeys[1]][pListKeys[2]][pListKeys[3]][pListKeys[4]][pListKeys[5]][pListKeys[6]][pListKeys[7]][pListKeys[8]][pListKeys[9]][pListKeys[10]]
         break
      default
         # wrap the value from the innermost key outwards, so that the path
         # is pListKeys[1] / pListKeys[2] / ... / pListKeys[pNumKeys]
         repeat with x = pNumKeys down to 1
            put empty into tArray
            put pValue into tArray[pListKeys[x]]
            put tArray into pValue
         end repeat

         union pArray with tArray recursively
         break
   end switch

   set the caseSensitive to false
end _builderArray


private function _jsonTextFromInput pJsonString
   # JSONParser has always taken "UTF-8 encoded JSON text". Accept it as
   # binary data (url "binfile:", tsNet, sockets), as text that holds those
   # bytes one character per byte (url "file:" and older code paths), or as
   # plain text, and return plain text.
   if pJsonString is strictly a binary string then
      return textDecode(pJsonString, "UTF-8")
   end if
   if pJsonString is an ascii string then
      return pJsonString
   end if

   # Text whose characters are really UTF-8 bytes survives being reassembled
   # into bytes and decoded as UTF-8; genuine text does not (it either has
   # characters outside the native set or its bytes are not valid UTF-8, and
   # textDecode drops invalid bytes, so re-encoding no longer matches).
   local tBytes, tText, tCheck
   put textEncode(pJsonString, "native") into tBytes
   if textDecode(tBytes, "native") is not pJsonString then
      return pJsonString
   end if
   put textDecode(tBytes, "UTF-8") into tText
   put textEncode(tText, "UTF-8") into tCheck
   if the number of bytes in tCheck is not the number of bytes in tBytes or tCheck is not tBytes then
      return pJsonString
   end if
   return tText
end _jsonTextFromInput


private function _unescapeJSONString pString
   # Decode the JSON escape sequences of one token in a single left to right
   # pass, so that the text produced by one escape is never read as the start
   # of another (ECMA-404 section 9). By the time a token gets here the
   # tokenizer pre-pass has turned \\ into \u005C and \" into \u0022.
   local tOut, tStart = 1, tOff, tNext, tHex, tCode, tLow, tLength

   put the number of chars in pString into tLength
   repeat forever
      put offset("\", pString, tStart - 1) into tOff
      if tOff is 0 then
         put char tStart to -1 of pString after tOut
         exit repeat
      end if
      add tStart - 1 to tOff
      put char tStart to (tOff - 1) of pString after tOut
      put char (tOff + 1) of pString into tNext
      put tOff + 2 into tStart

      switch tNext
         case "n"
            put numToChar(10) after tOut
            break
         case "r"
            put numToChar(13) after tOut
            break
         case "t"
            put numToChar(9) after tOut
            break
         case "b"
            put numToChar(8) after tOut
            break
         case "f"
            put numToChar(12) after tOut
            break
         case "/"
            put "/" after tOut
            break
         case quote
            put quote after tOut
            break
         case "\"
            put "\" after tOut
            break
         case "u"
            put char (tOff + 2) to (tOff + 5) of pString into tHex
            if not matchText(tHex, "^[0-9A-Fa-f]{4}$") then
               # malformed escape: keep it as written
               put "\u" after tOut
               break
            end if
            put baseConvert(tHex, 16, 10) into tCode
            put tOff + 6 into tStart
            # a high surrogate followed by a low surrogate is one code point
            if tCode >= 55296 and tCode <= 56319 and char tStart to (tStart + 1) of pString is "\u" then
               put char (tStart + 2) to (tStart + 5) of pString into tHex
               if matchText(tHex, "^[0-9A-Fa-f]{4}$") then
                  put baseConvert(tHex, 16, 10) into tLow
                  if tLow >= 56320 and tLow <= 57343 then
                     put 65536 + (tCode - 55296) * 1024 + (tLow - 56320) into tCode
                     add 6 to tStart
                  end if
               end if
            end if
            put numToCodepoint(tCode) after tOut
            break
         default
            # not a JSON escape: keep the backslash and the character
            put "\" & tNext after tOut
      end switch
   end repeat

   return tOut
end _unescapeJSONString


# json beautiful and compact
private function _jsonPrettyCompact pJsonString, pIndentSize, pForceRootType
   local tFormatted, tIndent, kSpace, tPrevToken = "", tIndex = 0, tItems = "", tHyphenMinus = false
   # tIsObject records, per open container, whether it is an object; tExpectKey
   # is true while the next token is a key, which must always be written as a
   # string however it looks ("123", "true", "null" or "" are all valid keys).
   local tIsObject = "", tExpectKey = false

   if sMaxLength is not an integer then put 50 into sMaxLength
   if pIndentSize is not a integer or pIndentSize > 10 then put 4 into pIndentSize

   repeat for pIndentSize
      put space after kSpace
   end repeat

   # escaped quotes < " > and reverse-solidus < \\ > will confuse the tokenizer
   # so we replace them with "\u0022" and "\u005C" which we'll unescape
   # with all the other unescapes when we parse
   put replaceText(pJsonString, "\\\\", "\u005C") into pJsonString
   put replaceText(pJsonString, "\\" & quote, "\u0022") into pJsonString

   // First pass: the text length of each object and list, by token index of
   // its opening bracket, from a running total of token lengths. (Measuring
   // with "the token a to b of pJsonString" re-tokenised the whole document
   // once per container, which took minutes on a few megabytes.) An empty
   // string token is not a bracket, so brackets are compared exactly.
   local tPos = 0, tStartPos, tLength
   repeat for each token tToken in pJsonString
      add 1 to tIndex
      add the number of chars in tToken + 1 to tPos
      if tToken is "[" or tToken is "{" then
         put comma & tIndex after tItems
         put tPos into tStartPos[tIndex]
      else if tToken is "]" or tToken is "}" then
         put tPos - tStartPos[the item - 1 of tItems] into tLength[the item - 1 of tItems]
         delete the item - 1 of tItems
      end if
   end repeat

   put 0 into tIndex
   put empty into tItems

   repeat for each token tToken in pJsonString
      add 1 to tIndex

      switch tToken
         case "{"; case "["
            put kSpace after tIndent
            put comma & (tToken is "{") after tIsObject
            put (tToken is "{") into tExpectKey
            if tLength[tIndex] > sMaxLength then
               put tToken & cr & tIndent after tFormatted
               put comma & true after tItems
            else
               put tToken after tFormatted
               put comma & false after tItems
            end if
            break

         case "}"; case "]"
            delete char - pIndentSize to - 1 of tIndent
            if the last item of tItems is true then
               put cr & tIndent & tToken after tFormatted
            else
               put tToken after tFormatted
            end if
            delete last item of tItems
            delete last item of tIsObject
            put false into tExpectKey
            break

         case ":"
            put ": " after tFormatted
            put false into tExpectKey
            break

         case comma
            if the last item of tItems is true then
               put comma & cr & tIndent after tFormatted
            else
               put comma & space after tFormatted
            end if
            put (the last item of tIsObject is true) into tExpectKey
            break

         case "-"
            put true into tHyphenMinus
            next repeat
            break

         default
            if tHyphenMinus then
               put "-" & tToken into tToken
               put false into tHyphenMinus
            end if

            if "\u00" is in tToken then
               replace "\u005C" with "\\" in tToken
               replace "\u0022" with "\" & quote in tToken
            end if

            if tExpectKey or pForceRootType is "string" then
               put quote & tToken & quote after tFormatted
            else
               put _jsonValue(tToken, true) after tFormatted
            end if
      end switch

      put tToken into tPrevToken
   end repeat

   return textDecode(tFormatted, "UTF8")
end _jsonPrettyCompact


private function _validateJSON pJsonString
   return (the token 1 of pJsonString is "{" and the token - 1 of pJsonString is "}") or (the token 1 of pJsonString is "[" or the token - 1 of pJsonString is "]")
end _validateJSON


private function _filterHandlerList pList
   filter lines of pList without regex pattern "^P.*"
   return pList
end _filterHandlerList


// The C0 control characters, which JSON requires to be escaped (ECMA-404
// section 9), and DEL. In the UTF-8 bytes that JSONStringify builds these
// are single bytes that never occur inside a multibyte sequence, so they
// can be matched as single native characters. The C1 controls U+0080 to
// U+009F are handled separately below: in UTF-8 each is the two-byte
// sequence C2 80 to C2 9F. Matching a bare byte 128 to 159, as the old
// list did, hits the continuation byte of any accented letter ("É" is
// C3 89), which is what wrote "É" as "\u0089". Typographic quotes,
// dashes and the ellipsis are printable and are no longer escaped.
constant kNonPrintableChars = "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,127"
private function _cleaningNonPrintableChars pText
   local tOldCase, i, tPair
   put the caseSensitive into tOldCase
   set the caseSensitive to true

   repeat for each item i in kNonPrintableChars
      if numToChar(i) is in pText then
         replace numToChar(i) with _convertAsciiToUnicodeNum(i) in pText
      end if
   end repeat

   repeat with i = 128 to 159
      put numToChar(194) & numToChar(i) into tPair
      if tPair is in pText then
         replace tPair with _convertAsciiToUnicodeNum(i) in pText
      end if
   end repeat

   set the caseSensitive to tOldCase
   return pText
end _cleaningNonPrintableChars


private function _resetNonPrintableChars pText
   local tOldCase, tUnicode, i

   if "\u00" is not in pText then
      return pText
   end if

   put the caseSensitive into tOldCase
   set the caseSensitive to true

   repeat for each item i in kNonPrintableChars
      put _convertAsciiToUnicodeNum(i) into tUnicode
      if tUnicode is in pText then
         replace tUnicode with numToChar(i) in pText
      end if
   end repeat

   repeat with i = 128 to 159
      put _convertAsciiToUnicodeNum(i) into tUnicode
      if tUnicode is in pText then
         replace tUnicode with (numToChar(194) & numToChar(i)) in pText
      end if
   end repeat

   set the caseSensitive to tOldCase
   return pText
end _resetNonPrintableChars


private function _convertAsciiToUnicodeNum pNum
   put baseConvert(pNum, 10, 16) into pNum
   switch the length of pNum
      case 1
         return "\u000" & pNum
         break
      case 2
         return "\u00" & pNum
         break
      case 3
         return "\u0" & pNum
         break
      case 4
         return "\u" & pNum
         break
   end switch
end _convertAsciiToUnicodeNum
