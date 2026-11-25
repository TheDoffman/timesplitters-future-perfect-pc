using System;
using System.Collections.Generic;
using System.Linq;

namespace TimeSplitters.Launcher
{
    // Environment-independent policy, exercised by both launcher test hosts.
    public sealed class HandheldProfile
    {
        public bool Enabled,GamingMode;
        static readonly string[] options={"--deck","--gaming-mode","--desktop-mode"};
        public static string[] Forward(string[] args){return options.Where(option=>(args??new string[0]).Contains(option)).ToArray();}
        public static bool HelperArguments(string[] args){return args.Length>=3&&args.Length<=6&&args.Skip(3).All(option=>options.Contains(option))&&args.Skip(3).Distinct().Count()==args.Length-3;}
        public static HandheldProfile Resolve(string[] args,Func<string,string> env,string product)
        {
            bool gaming=(env("XDG_CURRENT_DESKTOP")??"").IndexOf("gamescope",StringComparison.OrdinalIgnoreCase)>=0 || (env("XDG_SESSION_DESKTOP")??"").IndexOf("gamescope",StringComparison.OrdinalIgnoreCase)>=0;
            bool device=env("SteamDeck")=="1" || new[]{"Jupiter","Galileo","Steam Deck"}.Any(name=>(product??"").IndexOf(name,StringComparison.OrdinalIgnoreCase)>=0);
            if(args.Contains("--gaming-mode"))gaming=true;
            if(args.Contains("--desktop-mode"))gaming=false;
            return new HandheldProfile {Enabled=device||gaming||args.Contains("--deck"),GamingMode=gaming};
        }
    }
    public enum PadCommand { None,Up,Down,Left,Right,Accept,Back,Previous,Next,Keyboard,ScrollUp,ScrollDown }
    public sealed class HandheldPadState
    {
        bool neutral;int previous;PadCommand held;long repeatAt;
        public void Reset(){neutral=false;previous=0;held=PadCommand.None;repeatAt=0;}
        public PadCommand Read(int buttons,int x,int y,int scrollY,bool active,long milliseconds)
        {
            if(!active){Reset();return PadCommand.None;}
            bool atRest=buttons==0&&Math.Abs(x)<14000&&Math.Abs(y)<14000&&Math.Abs(scrollY)<14000;
            if(!neutral){if(atRest)neutral=true;previous=buttons;return PadCommand.None;}
            int pressed=buttons&~previous;previous=buttons;
            foreach(var pair in new[]{Tuple.Create(0,PadCommand.Accept),Tuple.Create(1,PadCommand.Back),Tuple.Create(3,PadCommand.Keyboard),Tuple.Create(9,PadCommand.Previous),Tuple.Create(10,PadCommand.Next)})
                if((pressed&(1<<pair.Item1))!=0)return pair.Item2;
            var direction=(buttons&(1<<11))!=0||y<-16000?PadCommand.Up:(buttons&(1<<12))!=0||y>16000?PadCommand.Down:(buttons&(1<<13))!=0||x<-16000?PadCommand.Left:(buttons&(1<<14))!=0||x>16000?PadCommand.Right:scrollY<-16000?PadCommand.ScrollUp:scrollY>16000?PadCommand.ScrollDown:PadCommand.None;
            if(direction==PadCommand.None){held=direction;return direction;}
            if(direction!=held){held=direction;repeatAt=milliseconds+360;return direction;}
            if(milliseconds<repeatAt)return PadCommand.None;repeatAt=milliseconds+130;return direction;
        }
    }
}
