package com.hexadecinull.vineos.di

import android.content.Context
import androidx.room.Room
import com.hexadecinull.vineos.data.repository.VMInstanceDao
import com.hexadecinull.vineos.data.repository.VineDatabase
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object DatabaseModule {
    @Provides
    @Singleton
    fun provideDatabase(@ApplicationContext context: Context): VineDatabase =
        Room.databaseBuilder(context, VineDatabase::class.java, VineDatabase.DATABASE_NAME)
            // Non-destructive: v1->v2 is covered by MIGRATION_1_2 (adds
            // enableVpnIsolation). Only a downgrade may drop tables; an
            // upgrade must never wipe user instances.
            .addMigrations(VineDatabase.MIGRATION_1_2)
            .fallbackToDestructiveMigrationOnDowngrade()
            .build()

    @Provides
    fun provideVMInstanceDao(db: VineDatabase): VMInstanceDao = db.vmInstanceDao()
}
