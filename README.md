# GPUSPPM
![alt text](https://github.com/dat-boi-lux/GPUSPPM/blob/main/Screenshot_2021-11-15_19-28-56.png)\
\
This is the source code and build instructions for **GPUSPPM** (*Stochastic Progressive Photon Mapping* implemented in **OpenGL** designed to run natively on a **GPU**).\
\
This code was originally written by **Toshiya Hachisuka** https://cs.uwaterloo.ca/~thachisu/ some time around 2011 and was originally hosted here: (The website has long been offline, but a version can be found on the Internet Archive here: https://web.archive.org/web/20141216214724/http://bee-www.com/) \
\
**NOTE:** I did not write any of this code, I simply edited the source to make it compile on Linux. I am also uploading it to Github to make it accessible to everyone.\
\
The source for a **Windows** compile and a **Linux** compile are provided. A prebuilt binary for Windows is also provided (this was compiled by Toshiya Hachisuka).

# Build Instructions - Linux/Ubuntu:
It is assumed that you have already installed the appropriate drivers for your GPU. This program will not run if you haven't done this.\
\
There are a few dependencies we will need to install in order to make this run. **GPUSPPM** requires **OpenGL**, **FreeGLUT**, and **GLEW**. Install them by typing this into a terminal:\
\
```sudo apt install freeglut3 freeglut3-dev libglew-dev```\
```sudo apt install mesa-utils```\
\
If you already have these in your system, you will get a *warning*, this is fine.\
\
Once you have successfully installed all of the dependencies, you can now move on to downloading this repository and compiling the source code.\
\
Clone this repository by typing this into a terminal:\
\
```git clone https://github.com/dat-boi-lux/GPUSPPM.git```\
\
Once the repository has downloaded onto your system, navigate to this folder (On Ubuntu, this folder should be somewhere in your *Home* directory and should be named **GPUSPPM**).\
\
Then open a terminal window in this directory. **Note:** In *Linux Mint* and possibly other graphical Linux distros, you can do this by: **Right-Clicking** inside your file explorer window and selecting **Open in Terminal**.\
\
It is time to compile the source code. We need to tell our c++ compiler (**GCC**) what libraries to use in order for this to compile properly. I.e. Type this into the terminal window you opened in the last step:\
\
```g++ gpusppmLINUX.cpp -o gpusppm -lGL -lGLU -lglut```\
\
You should now have successfully compiled **GPUSPPM**.

# Original Japanese Description - Hachisuka, T.

*"質問が結構来ていた，フルスペクトルなレンダリングとぼけた反射の例を追加しました．古いバージョンもまだダウンロードできます． RGBとスペクトルの相互変換はガウス分布で近似したものを使っているので，XYZなどの曲線のテーブルが必要なくなります． 最適化のコードを書いてなるべくエラーが小さくなるように近似したので，RGBとスペクトルの相互変換コードは他にも役に立つかもしれません． ガウス分布と最大値・最小値へのクランプで近似していますが，どうもフィッティングをうまくやればクランプは必要なさそうなので，そのうちこっそり差し替えるかも．*

*GLSLは各社のコンパイラの挙動が違いすぎてヤバイということにようやく気がつきました．JITコンパイラをベンダに任せるのは駄目なんじゃないだろうか． HLSLみたいにコンパイラ部分は標準で提供して，もっと低レベル部分を各ベンダに任せるほうがいいのでは？少なくともHLSLではGLSLほどヤバイ違いには直面したことがありません． まだ使ったことないですが，DirectComputeも同じ仕組みだと思います．OpenCL大丈夫か？*

*full-spectral renderingは空気感が出る，もしくは光を波として扱うなどと思った・書いたことがある人は，パストレーシングで点光源のコースティクスをレンダリングする刑に処します."*
