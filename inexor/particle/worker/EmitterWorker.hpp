/*
 * EmitterWorker.h
 *
 *  Created on: 01.02.2015
 *      Author: aschaeffer
 */

#ifndef SRC_ENGINE_ENTITY_SUBSYSTEM_PARTICLE_WORKER_EMITTERWORKER_H_
#define SRC_ENGINE_ENTITY_SUBSYSTEM_PARTICLE_WORKER_EMITTERWORKER_H_

#include "inexor/entity/manager/EntityInstanceManager.hpp"
#include "inexor/entity/manager/RelationshipInstanceManager.hpp"
#include "inexor/particle/worker/ParticleWorker.hpp"
#include "inexor/util/Logging.hpp"

namespace inexor {
namespace entity {
namespace particle {

    /**
     * Specializes ParticleWorker for emitter instances.
     */
    class EmitterWorker : public ParticleWorker
    {
        public:
            EmitterWorker(std::string name, int maxfps, FunctionRefPtr function, InstanceRefPtr<EntityInstance> emitter_instance, std::shared_ptr<EntityInstanceManager> entity_instance_manager, std::shared_ptr<RelationshipInstanceManager> relationship_instance_manager);
            virtual ~EmitterWorker();

            void Start();
            void Stop();
            static int Work(void *data);

        private:
            /**
             * The particle emitter instance.
             */
            InstanceRefPtr<EntityInstance> emitter_instance;

            /**
             * The entity instance manager.
             */
            std::shared_ptr<EntityInstanceManager> entity_instance_manager;

            /**
             * The relationship instance manager.
             */
            std::shared_ptr<RelationshipInstanceManager> relationship_instance_manager;

            /**
             * The particle pool.
             */
            std::deque<InstanceRefPtr<EntityInstance> > particle_pool;

            /**
             * The emitted particles are of this type.
             * TODO: track changes on the emitter_instance to this reference!
             */
            TypeRefPtr<EntityType> particle_type;

            /**
             * The relationship type:
             *
             *     particle--[:emitted_by]-->emitter
             *
             */
            TypeRefPtr<RelationshipType> emitted_by;

            /**
             * The relationship type:
             *
             *     emitter--[:apply_initializer]-->initializer
             *
             */
            TypeRefPtr<RelationshipType> apply_initializer;

            /**
             * The relationship type:
             *
             *     emitter--[:apply_modifier]-->modifier
             *
             */
            TypeRefPtr<RelationshipType> apply_modifier;

            /**
             * The relationship type:
             *
             *     emitter--[:apply_renderer]-->renderer
             *
             */
            TypeRefPtr<RelationshipType> apply_renderer;

            /**
             * The relationship type:
             *
             *     modifier--[:modifies]-->particle
             */
            TypeRefPtr<RelationshipType> modifies;

            /**
             * The relationship type:
             *
             *     renderer--[:renders]-->particle
             */
            TypeRefPtr<RelationshipType> renders;

    };

}
}
}

#endif /* SRC_ENGINE_ENTITY_SUBSYSTEM_PARTICLE_WORKER_EMITTERWORKER_H_ */
