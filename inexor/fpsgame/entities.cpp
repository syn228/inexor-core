#include <math.h>                                     // for sinf
#include <stddef.h>                                   // for size_t
#include <memory>                                     // for __shared_ptr

#include <enet/enet.h>                                // for _ENetPacketFlag...
#include "inexor/client/network.hpp"                  // for flushclient
#include "inexor/engine/particles.hpp"                // for ::PART_HUD_ICON...
#include "inexor/engine/renderparticles.hpp"          // for particle_icon
#include "inexor/engine/world.hpp"                    // for renderentarrow
#include "inexor/fpsgame/ai.hpp"                      // for inferwaypoints
#include "inexor/fpsgame/client.hpp"                  // for addmsg, sendpos...
#include "inexor/fpsgame/entities.hpp"
#include "inexor/fpsgame/fps.hpp"                     // for player1, msgsound
#include "inexor/fpsgame/fpsent.hpp"                  // for fpsent
#include "inexor/fpsgame/guns.hpp"                    // for itemstat, items...
#include "inexor/fpsgame/player.hpp"                  // for isthirdperson
#include "inexor/gamemode/gamemode.hpp"               // for m_bomb, m_noammo
#include "inexor/io/Logging.hpp"                      // for Log, Logger
#include "inexor/model/model.hpp"                     // for mapmodelname
#include "inexor/network/SharedVar.hpp"               // for SharedVar
#include "inexor/network/legacy/buffer_types.hpp"     // for packetbuf
#include "inexor/network/legacy/cube_network.hpp"     // for putint, DMF
#include "inexor/network/legacy/game_types.hpp"       // for ::N_EDITENT
#include "inexor/physics/physics.hpp"                 // for overlapsdynent
#include "inexor/shared/command.hpp"                  // for execute, idente...
#include "inexor/shared/cube_formatting.hpp"          // for defformatstring
#include "inexor/shared/cube_loops.hpp"               // for i, loopv, loopi
#include "inexor/shared/geom.hpp"                     // for vec, vec::(anon...
#include "inexor/sound/sound.hpp"                     // for playsound, ::S_...
#include "inexor/util/legacy_time.hpp"                // for lastmillis


using namespace inexor::sound;

namespace entities
{
    using namespace game;

    int extraentinfosize() { return 0; }       // size in bytes of what the 2 methods below read/write... so it can be skipped by other games

    void writeent(entity &e, char *buf)   // write any additional data to disk (except for ET_ ents)
    {
    }

    void readent(entity &e, char *buf, int ver)     // read from disk, and init
    {
        if(ver <= 30) switch(e.type)
        {
            case FLAG:
            case MONSTER:
            case TELEDEST:
            case RESPAWNPOINT:
            case BOX:
            case BARREL:
            case OBSTACLE:
            case PLATFORM:
            case ELEVATOR:
                e.attr1 = (int(e.attr1)+180)%360;
                break;
        }
        if(ver <= 31) switch(e.type)
        {
            case BOX:
            case BARREL:
            case OBSTACLE:
            case PLATFORM:
            case ELEVATOR:
                int yaw = (int(e.attr1)%360 + 360)%360 + 7; 
                e.attr1 = yaw - yaw%15;
                break;
        }
    }

#ifndef STANDALONE
    vector<extentity *> ents;

    vector<extentity *> &getents() { return ents; }

    bool mayattach(extentity &e) { return false; }
    bool attachent(extentity &e, extentity &a) { return false; }

    const char *itemname(int i)
    {
        int t = ents[i]->type;
        if(t<I_SHELLS || t>I_QUAD) return nullptr;
            return itemstats[t-I_SHELLS].name;
    }

    int itemicon(int i)
    {
        int t = ents[i]->type;
        if(t<I_SHELLS || t>I_QUAD) return -1;
            return itemstats[t-I_SHELLS].icon;
    }

    const char *entmdlname(int type)
    {
        static const char * const entmdlnames[] =
        {
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            "item/ammo/shell", "item/ammo/bullet", "item/ammo/rocket", "item/ammo/rifleround", "item/ammo/grenade", "item/ammo/cartridge",
            "item/ammo/bomb", "item/ammo/bombradius", "item/ammo/bombdelay",
            "item/health", "item/boost", "item/armor/green", "item/armor/yellow", "item/quad", "game/teleporter",
            nullptr, nullptr,
            "carrot",
            nullptr, nullptr,
            "checkpoint",
            nullptr, nullptr,
            nullptr, nullptr,
            nullptr,
            nullptr, //obstacle
        };
        return entmdlnames[type];
    }

    const char *entmodel(const entity &e)
    {
        if(e.type == TELEPORT)
        {
            if(e.attr2 > 0) return mapmodelname(e.attr2);
            if(e.attr2 < 0) return nullptr;
        }
        return e.type < MAXENTTYPES ? entmdlname(e.type) : nullptr;
    }

    void preloadentities()
    {
        loopi(MAXENTTYPES)
        {
            switch(i)
            {
                case I_SHELLS: case I_BULLETS: case I_ROCKETS: case I_ROUNDS: case I_GRENADES: case I_CARTRIDGES:
                    if(m_noammo) continue;
                    break;
                case I_HEALTH: case I_BOOST: case I_GREENARMOUR: case I_YELLOWARMOUR: case I_QUAD:
                    if(m_noitems) continue;
                    break;
                case I_BOMBS: case I_BOMBRADIUS: case I_BOMBDELAY:
                    if(!m_bomb) continue;
                    break;
            }
            const char *mdl = entmdlname(i);
            if(!mdl) continue;
            preloadmodel(mdl);
        }
        loopv(ents)
        {
            extentity &e = *ents[i];
            switch(e.type)
            {
                case TELEPORT:
                    if(e.attr2 > 0) preloadmodel(mapmodelname(e.attr2));
                case JUMPPAD:
                    if(e.attr4 > 0) preloadmapsound(e.attr4);
                    break;
            }
        }
    }

    void renderentities()
    {
        loopv(ents)
        {
            extentity &e = *ents[i];
            int revs = 10;
            switch(e.type)
            {
                case CARROT:
                case RESPAWNPOINT:
                    if(e.attr2) revs = 1;
                    break;
                case TELEPORT:
                    if(e.attr2 < 0) continue;
                    break;
                default:
                    if(!e.spawned() || e.type < I_SHELLS || e.type > I_QUAD) continue;
            }
            const char *mdlname = entmodel(e);
            if(mdlname)
            {
                vec p = e.o;
                p.z += 1+sinf(lastmillis/100.0+e.o.x+e.o.y)/20;
                rendermodel(&e.light, mdlname, ANIM_MAPMODEL|ANIM_LOOP, p, lastmillis/(float)revs, 0, MDL_SHADOW | MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);
            }
        }
    }

    void addammo(int type, int &v, bool local)
    {
        const itemstat &is = itemstats[type-I_SHELLS];
        v += is.add;
        if(v>is.max) v = is.max;
        if(local) msgsound(is.sound);
    }

    void repammo(fpsent *d, int type, bool local)
    {
        addammo(type, d->ammo[type-I_SHELLS+GUN_SG], local);
    }

    // these two functions are called when the server acknowledges that you really
    // picked up the item (in multiplayer someone may grab it before you).

    void pickupeffects(int n, fpsent *d)
    {
        using inexor::util::embraced;

        if(!ents.inrange(n)) return;
        int type = ents[n]->type;
        if(type<I_SHELLS || type>I_QUAD) return;
        ents[n]->clearspawned();
        if(!d) return;
        const itemstat &is = itemstats[type-I_SHELLS];
        if(d!=player1 || isthirdperson())
        {
            //particle_text(d->abovehead(), is.name, PART_TEXT, 2000, 0xFFC864, 4.0f, -8);
            // TODO: bomb items switch
            particle_icon(d->abovehead(), is.icon%4, is.icon/4, PART_HUD_ICON_GREY, 2000, 0xFFFFFF, 2.0f, -8);
        }
        playsound(itemstats[type-I_SHELLS].sound, d!=player1 ? &d->o : nullptr, nullptr, 0, 0, 0, -1, 0, 1500);
        d->pickup(type);
        if(d==player1) switch(type)
        {
            case I_BOOST:
                Log.game->info("you have a permanent +10 health bonus! ({})", d->maxhealth);
                playsound(S_V_BOOST, nullptr, nullptr, 0, 0, 0, -1, 0, 3000);
                break;

            case I_QUAD:
                Log.game->info("you got the quad!");
                playsound(S_V_QUAD, nullptr, nullptr, 0, 0, 0, -1, 0, 3000);
                break;

            case I_BOMBRADIUS:
                Log.game->info("you have a permanent +1 damage radius bonus!");
                playsound(S_V_QUAD, nullptr, nullptr, 0, 0, -1, 0, 3000); // TODO: other sound
                break;

            case I_BOMBDELAY:
                Log.game->info("your bombs explode faster!");
                playsound(S_ITEMHEALTH, nullptr, nullptr, 0, 0, -1, 0, 3000);
                break;
        }
    }

    // these functions are called when the client touches the item

    void teleporteffects(fpsent *d, int tp, int td, bool local)
    {
        if(ents.inrange(tp) && ents[tp]->type == TELEPORT)
        {
            extentity &e = *ents[tp];
            if(e.attr4 >= 0) 
            {
                int snd = S_TELEPORT, flags = 0;
                if(e.attr4 > 0) { snd = e.attr4; flags = SND_MAP; }
                if(d == player1) playsound(snd, nullptr, nullptr, flags);
                else
                {
                    playsound(snd, &e.o, nullptr, flags);
                    if(ents.inrange(td) && ents[td]->type == TELEDEST) playsound(snd, &ents[td]->o, nullptr, flags);
                }
            }
        }
        if(local && d->clientnum >= 0)
        {
            sendposition(d);
            packetbuf p(32, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_TELEPORT);
            putint(p, d->clientnum);
            putint(p, tp);
            putint(p, td);
            sendclientpacket(p.finalize(), 0);
            flushclient();
        }
    }

    void jumppadeffects(fpsent *d, int jp, bool local)
    {
        if(ents.inrange(jp) && ents[jp]->type == JUMPPAD)
        {
            extentity &e = *ents[jp];
            if(e.attr4 >= 0)
            {
                int snd = S_JUMPPAD, flags = 0;
                if(e.attr4 > 0) { snd = e.attr4; flags = SND_MAP; }
                if(d == player1) playsound(snd, nullptr, nullptr, flags);
                else playsound(snd, &e.o, nullptr, flags);
            }
        }
        if(local && d->clientnum >= 0)
        {
            sendposition(d);
            packetbuf p(16, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_JUMPPAD);
            putint(p, d->clientnum);
            putint(p, jp);
            sendclientpacket(p.finalize(), 0);
            flushclient();
        }
    }

    void teleport(int n, fpsent *d)     // also used by monsters
    {
        int e = -1, tag = ents[n]->attr1, beenhere = -1;
        for(;;)
        {
            e = findentity(TELEDEST, e+1);
            if(e==beenhere || e<0) { Log.std->warn("no teleport destination for tag {}", tag); return; } // TODO: LOG_N_TIMES(1)
            if(beenhere<0) beenhere = e;
            if(ents[e]->attr2==tag)
            {
                teleporteffects(d, n, e, true);
                d->o = ents[e]->o;
                d->yaw = ents[e]->attr1;
                if(ents[e]->attr3 > 0)
                {
                    vec dir;
                    vecfromyawpitch(d->yaw, 0, 1, 0, dir);
                    float speed = d->vel.magnitude2();
                    d->vel.x = dir.x*speed;
                    d->vel.y = dir.y*speed;
                }
                else d->vel = vec(0, 0, 0);
                entinmap(d);
                updatedynentcache(d);
                ai::inferwaypoints(d, ents[n]->o, ents[e]->o, 16.f);
                break;
            }
        }
    }

    void trypickup(int n, fpsent *d)
    {
        switch(ents[n]->type)
        {
            default:
                if(d->canpickup(ents[n]->type))
                {
                    addmsg(N_ITEMPICKUP, "rci", d, n);
                    ents[n]->clearspawned(); // even if someone else gets it first
                }
                break;

            case TELEPORT:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<500) break;
                if(ents[n]->attr3 > 0)
                {
                    defformatstring(hookname, "can_teleport_%d", ents[n]->attr3);
                    if(identexists(hookname) && !execute(hookname)) break;
                }
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                teleport(n, d);
                break;
            }

            case RESPAWNPOINT:
                if(d!=player1) break;
                if(n==respawnent) break;
                respawnent = n;
                Log.game->info("respawn point set!");
                playsound(S_V_RESPAWNPOINT);
                break;

            case JUMPPAD:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<300) break;
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                jumppadeffects(d, n, true);
                vec v((int)(char)ents[n]->attr3*10.0f, (int)(char)ents[n]->attr2*10.0f, ents[n]->attr1*12.5f);
                if(d->ai) d->ai->becareful = true;
                d->falling = vec(0, 0, 0);
                d->physstate = PHYS_FALL;
                d->timeinair = 1;
                d->vel = v;
                break;
            }
        }
    }

    void checkitems(fpsent *d)
    {
        if(d->state!=CS_ALIVE) return;
        vec o = d->feetpos();
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type==NOTUSED) continue;
            if(!e.spawned() && e.type!=TELEPORT && e.type!=JUMPPAD && e.type!=RESPAWNPOINT) continue;
            float dist = e.o.dist(o);
            if(dist<(e.type==TELEPORT ? 16 : 12)) trypickup(i, d);
        }
    }

    void checkquad(int time, fpsent *d)
    {
        if(d->quadmillis && (d->quadmillis -= time)<=0)
        {
            d->quadmillis = 0;
            playsound(S_PUPOUT, d==player1 ? nullptr : &d->o);
            if(d==player1) Log.game->info("quad damage is over");
        }
    }

    void putitems(packetbuf &p)            // puts items in network stream and also spawns them locally
    {
        putint(p, N_ITEMLIST);
        loopv(ents) {
            if((m_bomb && ents[i]->type>=I_BOMBS && ents[i]->type<=I_BOMBDELAY) || (!m_bomb && ents[i]->type>=I_SHELLS && ents[i]->type<=I_QUAD)) {
                putint(p, i);
                putint(p, ents[i]->type);
            }
            if(!m_noammo && ents[i]->type>=I_HEALTH && ents[i]->type<=I_QUAD) {
                putint(p, i);
                putint(p, ents[i]->type);
            }
        }
        putint(p, -1);
    }

    void resetspawns() { loopv(ents) ents[i]->clearspawned(); }

    void spawnitems(bool force)
    {
        if(m_noitems) return;
        loopv(ents) {
            if((m_bomb  && ents[i]->type>=I_BOMBS && ents[i]->type<=I_BOMBDELAY) || (!m_bomb && ents[i]->type>=I_SHELLS && ents[i]->type<=I_QUAD)) {
                ents[i]->setspawned(force || !delayspawn(ents[i]->type));
            }
            if(!m_noammo && ents[i]->type>=I_HEALTH && ents[i]->type<=I_QUAD) {
                ents[i]->setspawned(force || !delayspawn(ents[i]->type));
            }
        }
    }

    void setspawn(int i, bool on) { if(ents.inrange(i)) ents[i]->setspawned(on); }

    extentity *newentity() { return new fpsentity(); }
    void deleteentity(extentity *e) { delete (fpsentity *)e; }

    void clearents()
    {
        while(ents.length()) deleteentity(ents.pop());
    }

    enum
    {
        TRIG_COLLIDE    = 1<<0,
        TRIG_TOGGLE     = 1<<1,
        TRIG_ONCE       = 0<<2,
        TRIG_MANY       = 1<<2,
        TRIG_DISAPPEAR  = 1<<3,
        TRIG_AUTO_RESET = 1<<4,
        TRIG_RUMBLE     = 1<<5,
        TRIG_LOCKED     = 1<<6,
        TRIG_ENDSP      = 1<<7
    };

    static const int NUMTRIGGERTYPES = 32;

    static const int triggertypes[NUMTRIGGERTYPES] =
    {
        -1,
        TRIG_ONCE,                    // 1
        TRIG_RUMBLE,                  // 2
        TRIG_TOGGLE,                  // 3
        TRIG_TOGGLE | TRIG_RUMBLE,    // 4
        TRIG_MANY,                    // 5
        TRIG_MANY | TRIG_RUMBLE,      // 6
        TRIG_MANY | TRIG_TOGGLE,      // 7
        TRIG_MANY | TRIG_TOGGLE | TRIG_RUMBLE,    // 8
        TRIG_COLLIDE | TRIG_TOGGLE | TRIG_RUMBLE, // 9
        TRIG_COLLIDE | TRIG_TOGGLE | TRIG_AUTO_RESET | TRIG_RUMBLE, // 10
        TRIG_COLLIDE | TRIG_TOGGLE | TRIG_LOCKED | TRIG_RUMBLE,     // 11
        TRIG_DISAPPEAR,               // 12
        TRIG_DISAPPEAR | TRIG_RUMBLE, // 13
        TRIG_DISAPPEAR | TRIG_COLLIDE | TRIG_LOCKED, // 14
        -1 /* reserved 15 */,
        -1 /* reserved 16 */,
        -1 /* reserved 17 */,
        -1 /* reserved 18 */,
        -1 /* reserved 19 */,
        -1 /* reserved 20 */,
        -1 /* reserved 21 */,
        -1 /* reserved 22 */,
        -1 /* reserved 23 */,
        -1 /* reserved 24 */,
        -1 /* reserved 25 */,
        -1 /* reserved 26 */,
        -1 /* reserved 27 */,
        -1 /* reserved 28 */,
        TRIG_DISAPPEAR | TRIG_RUMBLE | TRIG_ENDSP, // 29
        -1 /* reserved 30 */,
        -1 /* reserved 31 */,
    };

    #define validtrigger(type) (triggertypes[(type) & (NUMTRIGGERTYPES-1)]>=0)
    #define checktriggertype(type, flag) (triggertypes[(type) & (NUMTRIGGERTYPES-1)] & (flag))

    static inline void cleartriggerflags(extentity &e)
    {
        e.flags &= ~(EF_ANIM | EF_NOVIS | EF_NOSHADOW | EF_NOCOLLIDE);
    }

    static inline void setuptriggerflags(fpsentity &e)
    {
        cleartriggerflags(e);
        e.flags |= EF_ANIM;
        if(checktriggertype(e.attr3, TRIG_COLLIDE|TRIG_DISAPPEAR)) e.flags |= EF_NOSHADOW;
        if(!checktriggertype(e.attr3, TRIG_COLLIDE)) e.flags |= EF_NOCOLLIDE;
        switch(e.triggerstate)
        {
            case TRIGGERING:
                if(checktriggertype(e.attr3, TRIG_COLLIDE) && lastmillis-e.lasttrigger >= 500) e.flags |= EF_NOCOLLIDE;
                break;
            case TRIGGERED:
                if(checktriggertype(e.attr3, TRIG_COLLIDE)) e.flags |= EF_NOCOLLIDE;
                break;
            case TRIGGER_DISAPPEARED:
                e.flags |= EF_NOVIS | EF_NOCOLLIDE;
                break;
        }
    }

    void resettriggers()
    {
        loopv(ents)
        {
            fpsentity &e = *(fpsentity *)ents[i];
            if(e.type != ET_MAPMODEL || !validtrigger(e.attr3)) continue;
            e.triggerstate = TRIGGER_RESET;
            e.lasttrigger = 0;
            setuptriggerflags(e);
        }
    }

    void unlocktriggers(int tag, int oldstate = TRIGGER_RESET, int newstate = TRIGGERING)
    {
        loopv(ents)
        {
            fpsentity &e = *(fpsentity *)ents[i];
            if(e.type != ET_MAPMODEL || !validtrigger(e.attr3)) continue;
            if(e.attr4 == tag && e.triggerstate == oldstate && checktriggertype(e.attr3, TRIG_LOCKED))
            {
                if(newstate == TRIGGER_RESETTING && checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) continue;
                e.triggerstate = newstate;
                e.lasttrigger = lastmillis;
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) playsound(S_RUMBLE, &e.o);
            }
        }
    }

    ICOMMAND(trigger, "ii", (int *tag, int *state),

    {
        if(*state) unlocktriggers(*tag);
        else unlocktriggers(*tag, TRIGGERED, TRIGGER_RESETTING);
    });

    VAR(triggerstate, -1, 0, 1);

    void doleveltrigger(int trigger, int state)
    {
        defformatstring(aliasname, "level_trigger_%d", trigger);
        if(identexists(aliasname))
        {
            triggerstate = state;
            execute(aliasname);
        }
    }

    void checktriggers()
    {
        if(player1->state != CS_ALIVE) return;
        vec o = player1->feetpos();
        loopv(ents)
        {
            fpsentity &e = *(fpsentity *)ents[i];
            if(e.type != ET_MAPMODEL || !validtrigger(e.attr3)) continue;
            switch(e.triggerstate)
            {
                case TRIGGERING:
                case TRIGGER_RESETTING:
                    if(lastmillis-e.lasttrigger>=1000)
                    {
                        if(e.attr4)
                        {
                            if(e.triggerstate == TRIGGERING) unlocktriggers(e.attr4);
                            else unlocktriggers(e.attr4, TRIGGERED, TRIGGER_RESETTING);
                        }
                        if(checktriggertype(e.attr3, TRIG_DISAPPEAR)) e.triggerstate = TRIGGER_DISAPPEARED;
                        else if(e.triggerstate==TRIGGERING && checktriggertype(e.attr3, TRIG_TOGGLE)) e.triggerstate = TRIGGERED;
                        else e.triggerstate = TRIGGER_RESET;
                    }
                    setuptriggerflags(e);
                    break;
                case TRIGGER_RESET:
                    if(e.lasttrigger)
                    {
                        if(checktriggertype(e.attr3, TRIG_AUTO_RESET|TRIG_MANY|TRIG_LOCKED) && e.o.dist(o)-player1->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                            e.lasttrigger = 0;
                        break;
                    }
                    else if(e.o.dist(o)-player1->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12)) break;
                    else if(checktriggertype(e.attr3, TRIG_LOCKED))
                    {
                        if(!e.attr4) break;
                        doleveltrigger(e.attr4, -1);
                        e.lasttrigger = lastmillis;
                        break;
                    }
                    e.triggerstate = TRIGGERING;
                    e.lasttrigger = lastmillis;
                    setuptriggerflags(e);
                    if(checktriggertype(e.attr3, TRIG_RUMBLE)) playsound(S_RUMBLE, &e.o);
                    if(checktriggertype(e.attr3, TRIG_ENDSP)); //endsp(false);
                    if(e.attr4) doleveltrigger(e.attr4, 1);
                    break;
                case TRIGGERED:
                    if(e.o.dist(o)-player1->radius<(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                    {
                        if(e.lasttrigger) break;
                    }
                    else if(checktriggertype(e.attr3, TRIG_AUTO_RESET))
                    {
                        if(lastmillis-e.lasttrigger<6000) break;
                    }
                    else if(checktriggertype(e.attr3, TRIG_MANY))
                    {
                        e.lasttrigger = 0;
                        break;
                    }
                    else break;
                    if(checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) break;
                    e.triggerstate = TRIGGER_RESETTING;
                    e.lasttrigger = lastmillis;
                    setuptriggerflags(e);
                    if(checktriggertype(e.attr3, TRIG_RUMBLE)) playsound(S_RUMBLE, &e.o);
                    if(checktriggertype(e.attr3, TRIG_ENDSP)); // endsp(false);
                    if(e.attr4) doleveltrigger(e.attr4, 0);
                    break;
            }
        }
    }

    void animatemapmodel(const extentity &e, int &anim, int &basetime)
    {
        const fpsentity &f = (const fpsentity &)e;
        if(validtrigger(f.attr3)) switch(f.triggerstate)
        {
            case TRIGGER_RESET: anim = ANIM_TRIGGER|ANIM_START; break;
            case TRIGGERING: anim = ANIM_TRIGGER; basetime = f.lasttrigger; break;
            case TRIGGERED: anim = ANIM_TRIGGER|ANIM_END; break;
            case TRIGGER_RESETTING: anim = ANIM_TRIGGER|ANIM_REVERSE; basetime = f.lasttrigger; break;
        }
    }

    void fixentity(extentity &e)
    {
        switch(e.type)
        {
            case FLAG:
            case BOX:
            case BARREL:
            case OBSTACLE:
            case PLATFORM:
            case ELEVATOR:
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
            case TELEDEST:
                e.attr3 = e.attr2;
            case MONSTER:
                e.attr2 = e.attr1;
            case RESPAWNPOINT:
                e.attr1 = (int)player1->yaw;
                break;
        }
    }

    void entradius(extentity &e, bool color)
    {
        switch(e.type)
        {
            case TELEPORT:
                loopv(ents) if(ents[i]->type == TELEDEST && e.attr1==ents[i]->attr2)
                {
                    renderentarrow(e, vec(ents[i]->o).sub(e.o).normalize(), e.o.dist(ents[i]->o));
                    break;
                }
                break;

            case JUMPPAD:
                renderentarrow(e, vec((int)(char)e.attr3*10.0f, (int)(char)e.attr2*10.0f, e.attr1*12.5f).normalize(), 4);
                break;

            case FLAG:
            case MONSTER:
            case TELEDEST:
            case RESPAWNPOINT:
            case BOX:
            case BARREL:
            case OBSTACLE:
            case PLATFORM:
            case ELEVATOR:
            {
                vec dir;
                vecfromyawpitch(e.attr1, 0, 1, 0, dir);
                renderentarrow(e, dir, 4);
                break;
            }
            case MAPMODEL:
                if(validtrigger(e.attr3)) renderentring(e, checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12);
                break;
        }
    }

    bool printent(extentity &e, char *buf, int len)
    {
        return false;
    }

    const char *entnameinfo(entity &e) { return ""; }
    const char *entname(int i)
    {
        static const char * const entnames[] =
        {
            "none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight",
            "shells", "bullets", "rockets", "riflerounds", "grenades", "cartridges",
            "bombs", "bombradius", "bombdelay",
            "health", "healthboost", "greenarmour", "yellowarmour", "quaddamage",
            "teleport", "teledest",
            "monster", "carrot", "jumppad",
            "base", "respawnpoint",
            "box", "barrel",
            "platform", "elevator",
            "flag",
            "obstacle",
            "", "", // two empty strings follows.
        };
        return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    }

    void editent(int i, bool local)
    {
        extentity &e = *ents[i];
        if(e.type == ET_MAPMODEL && validtrigger(e.attr3))
        {
            fpsentity &f = (fpsentity &)e;
            f.triggerstate = TRIGGER_RESET;
            f.lasttrigger = 0;
            setuptriggerflags(f);
        }
        else cleartriggerflags(e);
        if(local) addmsg(N_EDITENT, "rii3ii5", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
    }

    float dropheight(entity &e)
    {
        if(e.type==BASE || e.type==FLAG) return 0.0f;
        return 4.0f;
    }

    bool hasmapmodel(const extentity &e)
    {
        if(e.type==MAPMODEL || e.type==BOX || e.type==BARREL || e.type==OBSTACLE) return true;
        return false;
    }
#endif
}

