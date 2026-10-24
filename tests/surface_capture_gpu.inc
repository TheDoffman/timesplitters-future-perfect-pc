/* Included inside field_rounding_cases after the controlled trajectories.
 * Actual GPU input/first-pass/output captured in deferred execution order. */
{
    FILE *capture=fopen("live-water.bin","rb");CHECK(capture);
    char magic[8];CHECK(fread(magic,1,8,capture)==8 && !memcmp(magic,"TSWATER1",8));
    unsigned char *pixels=malloc(3u*SURFACE_SIDE*SURFACE_SIDE*4);CHECK(pixels);
    uint32_t words[23];unsigned records=0;
    while(fread(words,1,sizeof words,capture)==sizeof words) {
        CHECK(fread(pixels,1,3u*SURFACE_SIDE*SURFACE_SIDE*4,capture)==3u*SURFACE_SIDE*SURFACE_SIDE*4);
        CHECK(words[3]==4 && words[4]==7 && words[5]>0);
        float parameters[4];memcpy(parameters,words+19,sizeof parameters);
        printf("live-feedback record=%u frame=%u resource=%08x k=%g a=%g b=%g d=%g\n",
            records,words[1],words[0],parameters[0],parameters[1],parameters[2],parameters[3]);
        rhi_image_update(bytes[0].image,0,NULL,pixels,SURFACE_SIDE*4,SURFACE_SIDE*SURFACE_SIDE*4);
        rhi_image_update(first_byte.image,0,NULL,pixels+SURFACE_SIDE*SURFACE_SIDE*4,SURFACE_SIDE*4,SURFACE_SIDE*SURFACE_SIDE*4);
        RhiView *inputs[4]={bytes[0].read,bytes[0].read,bytes[0].read,bytes[0].read};
        round_draw(f,feedback[0],&offsets[0][0][0],&cb[0],inputs,4,&bytes[1]);
        field_read(bytes[1].image,2,field_a);
        double first_error=0,second_error=0;
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)
            first_error=fmax(first_error,fabs(field_a[i*4]-pixels[SURFACE_SIDE*SURFACE_SIDE*4+i*4]/255.f));
        inputs[0]=first_byte.read;inputs[1]=bytes[0].read;
        round_draw(f,feedback[1],&offsets[1][0][0],&cb[1],inputs,2,&bytes[1]);
        field_read(bytes[1].image,2,field_a);
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=1;channel<4;channel++)
            second_error=fmax(second_error,fabs(field_a[i*4+channel]-pixels[2*SURFACE_SIDE*SURFACE_SIDE*4+i*4+channel]/255.f));
        printf("live-original first_error=%.9g second_error=%.9g\n",first_error,second_error);
        CHECK(first_error==0 && second_error==0);
        round_draw(f,feedback[1],&offsets[1][0][0],&cb[1],inputs,2,&first_float);
        round_draw(f,residual,NULL,NULL,inputs,2,&rounding);
        const double pieces[3][3]={{1,0,0},{.5,.5,0},{5./12,5./12,1./6}};
        for(unsigned rate=0;rate<3;rate++) {
            round_draw(f,unpack,NULL,NULL,&bytes[0].read,1,&world);
            CHECK(pc_surface_gpu_begin(surface,world.read,rounding.read));
            for(unsigned part=0;part<=rate;part++)CHECK(pc_surface_gpu_advance(surface,pieces[rate][part]));
            RhiView *boundary_inputs[2]={world.read,first_float.read};
            round_draw(f,boundary,NULL,NULL,boundary_inputs,2,&bytes[1]);
            field_read(bytes[1].image,2,field_a);double error=0;
            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=1;channel<4;channel++)
                error=fmax(error,fabs(field_a[i*4+channel]-pixels[2*SURFACE_SIDE*SURFACE_SIDE*4+i*4+channel]/255.f));
            printf("live-coupled pieces=%u byte_error=%.9g\n",rate+1,error);CHECK(error==0);
        }
        records++;
    }
    CHECK(feof(capture) && records==16);fclose(capture);free(pixels);
    puts("PASS: captured live feedback matches original shaders and coupled fractional updates");
}
